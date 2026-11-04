/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.firebase.ump.internal.cpp;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import com.google.android.ump.ConsentDebugSettings;
import com.google.android.ump.ConsentForm;
import com.google.android.ump.ConsentForm.OnConsentFormDismissedListener;
import com.google.android.ump.ConsentInformation;
import com.google.android.ump.ConsentInformation.OnConsentInfoUpdateFailureListener;
import com.google.android.ump.ConsentInformation.OnConsentInfoUpdateSuccessListener;
import com.google.android.ump.ConsentInformation.PrivacyOptionsRequirementStatus;
import com.google.android.ump.ConsentRequestParameters;
import com.google.android.ump.FormError;
import com.google.android.ump.UserMessagingPlatform;
import com.google.android.ump.UserMessagingPlatform.OnConsentFormLoadFailureListener;
import com.google.android.ump.UserMessagingPlatform.OnConsentFormLoadSuccessListener;
import java.lang.ref.WeakReference;
import java.util.ArrayList;

/** Helper class between the UMP C++ wrapper and the Android UMP API. */
public class ConsentInfoHelper {
  private static final long CPP_NULLPTR = 0;

  private final Object mLock = new Object();
  private Context mAppContext;
  private final Handler mMainHandler = new Handler(Looper.getMainLooper());
  private long mInternalPtr = 0;
  private WeakReference<Activity> mActivityRef;
  private ConsentForm mConsentForm = null;

  public static final int PRIVACY_OPTIONS_REQUIREMENT_UNKNOWN =
      PrivacyOptionsRequirementStatus.UNKNOWN.ordinal();
  public static final int PRIVACY_OPTIONS_REQUIREMENT_REQUIRED =
      PrivacyOptionsRequirementStatus.REQUIRED.ordinal();
  public static final int PRIVACY_OPTIONS_REQUIREMENT_NOT_REQUIRED =
      PrivacyOptionsRequirementStatus.NOT_REQUIRED.ordinal();

  public static final int FUNCTION_REQUEST_CONSENT_INFO_UPDATE = 0;
  public static final int FUNCTION_LOAD_CONSENT_FORM = 1;
  public static final int FUNCTION_SHOW_CONSENT_FORM = 2;
  public static final int FUNCTION_LOAD_AND_SHOW_CONSENT_FORM_IF_REQUIRED = 3;
  public static final int FUNCTION_SHOW_PRIVACY_OPTIONS_FORM = 4;
  public static final int FUNCTION_COUNT = 5;

  public ConsentInfoHelper(long consentInfoInternalPtr, Activity activity) {
    synchronized (mLock) {
      mAppContext = activity != null ? activity.getApplicationContext() : null;
      mInternalPtr = consentInfoInternalPtr;
      mActivityRef = new WeakReference<>(activity);
    }
  }

  public void setActivity(Activity activity) {
    synchronized (mLock) {
      if (activity != null) {
        mActivityRef = new WeakReference<>(activity);
        if (mAppContext == null) mAppContext = activity.getApplicationContext();
      }
    }
  }

  private static boolean isActivityUsable(Activity a) {
    return a != null && !a.isFinishing() && (Build.VERSION.SDK_INT < 17 || !a.isDestroyed());
  }

  private Activity resolveActivityLocked(Activity explicitActivity) {
    if (isActivityUsable(explicitActivity)) return explicitActivity;
    Activity fallback = mActivityRef != null ? mActivityRef.get() : null;
    return isActivityUsable(fallback) ? fallback : null;
  }

  private Context getSafeContextLocked() {
    return mAppContext != null ? mAppContext : resolveActivityLocked(null);
  }

  private void completeWithErrorLocked(int fnId, long futureHandle, int code, String msg) {
    if (mInternalPtr != CPP_NULLPTR) completeFuture(fnId, mInternalPtr, futureHandle, code, msg);
  }

  private void completeFormError(int fnId, long futureHandle, FormError err) {
    synchronized (mLock) {
      if (err == null) {
        completeWithErrorLocked(fnId, futureHandle, 0, null);
      } else {
        completeWithErrorLocked(fnId, futureHandle, err.getErrorCode(), err.getMessage());
      }
    }
  }

  private OnConsentFormDismissedListener dismissedListener(
      final int fnId, final long futureHandle) {
    return new OnConsentFormDismissedListener() {
      @Override
      public void onConsentFormDismissed(FormError formError) {
        completeFormError(fnId, futureHandle, formError);
      }
    };
  }

  private interface ActivityAction {
    void run(Activity activity);
  }

  private void runOnActivity(
      final int fnId,
      final long futureHandle,
      Activity explicit,
      String nullMsg,
      final String destroyedMsg,
      final ConsentForm restoreFormOnCancel,
      final ActivityAction action) {
    final Activity target;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR) return;
      target = resolveActivityLocked(explicit);
      if (target == null) {
        if (restoreFormOnCancel != null && mConsentForm == null) mConsentForm = restoreFormOnCancel;
        completeWithErrorLocked(
            fnId, futureHandle, FormError.ErrorCode.INVALID_OPERATION, nullMsg);
        return;
      }
    }
    mMainHandler.post(
        new Runnable() {
          @Override
          public void run() {
            try {
              Activity cur;
              synchronized (mLock) {
                if (mInternalPtr == CPP_NULLPTR) return;
                cur = resolveActivityLocked(target);
                if (cur == null) {
                  if (restoreFormOnCancel != null && mConsentForm == null) {
                    mConsentForm = restoreFormOnCancel;
                  }
                  completeWithErrorLocked(
                      fnId, futureHandle, FormError.ErrorCode.INVALID_OPERATION, destroyedMsg);
                  return;
                }
              }
              action.run(cur);
            } catch (Throwable t) {
              synchronized (mLock) {
                completeWithErrorLocked(
                    fnId, futureHandle, FormError.ErrorCode.INTERNAL_ERROR, t.getMessage());
              }
            }
          }
        });
  }

  public int getConsentStatus() {
    Context ctx;
    synchronized (mLock) {
      ctx = getSafeContextLocked();
    }
    return ctx == null
        ? ConsentInformation.ConsentStatus.UNKNOWN
        : UserMessagingPlatform.getConsentInformation(ctx).getConsentStatus();
  }

  public void requestConsentInfoUpdate(
      final long futureHandle,
      boolean tagForUnderAgeOfConsent,
      int debugGeography,
      ArrayList<String> debugIdList) {
    final int fnId = FUNCTION_REQUEST_CONSENT_INFO_UPDATE;
    final Context safeContext;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR) return;
      safeContext = getSafeContextLocked();
    }
    ConsentDebugSettings.Builder dbg = null;
    if (safeContext != null) {
      if (debugGeography != ConsentDebugSettings.DebugGeography.DEBUG_GEOGRAPHY_DISABLED) {
        dbg = new ConsentDebugSettings.Builder(safeContext).setDebugGeography(debugGeography);
      }
      if (debugIdList != null && !debugIdList.isEmpty()) {
        if (dbg == null) dbg = new ConsentDebugSettings.Builder(safeContext);
        for (int i = 0; i < debugIdList.size(); i++) {
          String id = debugIdList.get(i);
          if (id != null) dbg = dbg.addTestDeviceHashedId(id);
        }
      }
    }
    ConsentRequestParameters.Builder pb =
        new ConsentRequestParameters.Builder().setTagForUnderAgeOfConsent(tagForUnderAgeOfConsent);
    if (dbg != null) pb = pb.setConsentDebugSettings(dbg.build());
    final ConsentRequestParameters params = pb.build();

    runOnActivity(
        fnId,
        futureHandle,
        null,
        "Cannot request consent info update: Activity is null or destroyed.",
        "Activity was destroyed before requestConsentInfoUpdate could run.",
        null,
        new ActivityAction() {
          @Override
          public void run(Activity cur) {
            UserMessagingPlatform.getConsentInformation(safeContext)
                .requestConsentInfoUpdate(
                    cur,
                    params,
                    new OnConsentInfoUpdateSuccessListener() {
                      @Override
                      public void onConsentInfoUpdateSuccess() {
                        completeFormError(fnId, futureHandle, null);
                      }
                    },
                    new OnConsentInfoUpdateFailureListener() {
                      @Override
                      public void onConsentInfoUpdateFailure(FormError e) {
                        synchronized (mLock) {
                          completeWithErrorLocked(
                              fnId,
                              futureHandle,
                              e != null ? e.getErrorCode() : FormError.ErrorCode.INTERNAL_ERROR,
                              e != null ? e.getMessage() : "Unknown error");
                        }
                      }
                    });
          }
        });
  }

  public void loadConsentForm(final long futureHandle) {
    final int fnId = FUNCTION_LOAD_CONSENT_FORM;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR) return;
      if (getSafeContextLocked() == null) {
        completeWithErrorLocked(
            fnId,
            futureHandle,
            FormError.ErrorCode.INVALID_OPERATION,
            "Cannot load consent form: Context is null.");
        return;
      }
    }
    mMainHandler.post(
        new Runnable() {
          @Override
          public void run() {
            try {
              Context ctx;
              synchronized (mLock) {
                if (mInternalPtr == CPP_NULLPTR) return;
                ctx = getSafeContextLocked();
                if (ctx == null) {
                  completeWithErrorLocked(
                      fnId,
                      futureHandle,
                      FormError.ErrorCode.INVALID_OPERATION,
                      "Context was destroyed before loadConsentForm could run.");
                  return;
                }
              }
              UserMessagingPlatform.loadConsentForm(
                  ctx,
                  new OnConsentFormLoadSuccessListener() {
                    @Override
                    public void onConsentFormLoadSuccess(ConsentForm form) {
                      synchronized (mLock) {
                        if (mInternalPtr != CPP_NULLPTR) {
                          mConsentForm = form;
                          completeWithErrorLocked(fnId, futureHandle, 0, null);
                        }
                      }
                    }
                  },
                  new OnConsentFormLoadFailureListener() {
                    @Override
                    public void onConsentFormLoadFailure(FormError e) {
                      synchronized (mLock) {
                        if (mInternalPtr != CPP_NULLPTR) {
                          mConsentForm = null;
                          completeWithErrorLocked(
                              fnId,
                              futureHandle,
                              e != null ? e.getErrorCode() : FormError.ErrorCode.INTERNAL_ERROR,
                              e != null ? e.getMessage() : "Unknown error");
                        }
                      }
                    }
                  });
            } catch (Throwable t) {
              synchronized (mLock) {
                completeWithErrorLocked(
                    fnId, futureHandle, FormError.ErrorCode.INTERNAL_ERROR, t.getMessage());
              }
            }
          }
        });
  }

  public boolean showConsentForm(final long futureHandle, final Activity activity) {
    final int fnId = FUNCTION_SHOW_CONSENT_FORM;
    final ConsentForm form;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR || mConsentForm == null) return false;
      if (resolveActivityLocked(activity) == null) {
        completeWithErrorLocked(
            fnId,
            futureHandle,
            FormError.ErrorCode.INVALID_OPERATION,
            "Cannot show consent form: Activity is null or destroyed.");
        return true;
      }
      form = mConsentForm;
      mConsentForm = null;
    }
    runOnActivity(
        fnId,
        futureHandle,
        activity,
        "Cannot show consent form: Activity is null or destroyed.",
        "Activity was destroyed before showConsentForm could run.",
        form,
        new ActivityAction() {
          @Override
          public void run(Activity cur) {
            form.show(cur, dismissedListener(fnId, futureHandle));
          }
        });
    return true;
  }

  public void loadAndShowConsentFormIfRequired(final long futureHandle, final Activity activity) {
    final int fnId = FUNCTION_LOAD_AND_SHOW_CONSENT_FORM_IF_REQUIRED;
    runOnActivity(
        fnId,
        futureHandle,
        activity,
        "Cannot load and show consent form: Activity is null or destroyed.",
        "Activity was destroyed before loadAndShowConsentFormIfRequired could run.",
        null,
        new ActivityAction() {
          @Override
          public void run(Activity cur) {
            UserMessagingPlatform.loadAndShowConsentFormIfRequired(
                cur, dismissedListener(fnId, futureHandle));
          }
        });
  }

  public int getPrivacyOptionsRequirementStatus() {
    Context ctx;
    synchronized (mLock) {
      ctx = getSafeContextLocked();
    }
    return ctx == null
        ? PRIVACY_OPTIONS_REQUIREMENT_UNKNOWN
        : UserMessagingPlatform.getConsentInformation(ctx)
            .getPrivacyOptionsRequirementStatus()
            .ordinal();
  }

  public void showPrivacyOptionsForm(final long futureHandle, final Activity activity) {
    final int fnId = FUNCTION_SHOW_PRIVACY_OPTIONS_FORM;
    runOnActivity(
        fnId,
        futureHandle,
        activity,
        "Cannot show privacy options form: Activity is null or destroyed.",
        "Activity was destroyed before showPrivacyOptionsForm could run.",
        null,
        new ActivityAction() {
          @Override
          public void run(Activity cur) {
            UserMessagingPlatform.showPrivacyOptionsForm(
                cur, dismissedListener(fnId, futureHandle));
          }
        });
  }

  public boolean canRequestAds() {
    Context ctx;
    synchronized (mLock) {
      ctx = getSafeContextLocked();
    }
    return ctx != null && UserMessagingPlatform.getConsentInformation(ctx).canRequestAds();
  }

  public boolean isConsentFormAvailable() {
    Context ctx;
    synchronized (mLock) {
      ctx = getSafeContextLocked();
    }
    return ctx != null && UserMessagingPlatform.getConsentInformation(ctx).isConsentFormAvailable();
  }

  public void reset() {
    Context ctx;
    synchronized (mLock) {
      mConsentForm = null;
      ctx = getSafeContextLocked();
    }
    if (ctx != null) UserMessagingPlatform.getConsentInformation(ctx).reset();
  }

  public void disconnect() {
    synchronized (mLock) {
      mInternalPtr = CPP_NULLPTR;
      mConsentForm = null;
    }
    mMainHandler.removeCallbacksAndMessages(null);
  }

  public static native void completeFuture(
      int futureFn, long nativeInternalPtr, long futureHandle, int errorCode, String errorMessage);
}
