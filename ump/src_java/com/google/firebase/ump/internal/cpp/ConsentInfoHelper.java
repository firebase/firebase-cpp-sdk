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

/**
 * Helper class to make interactions between the UMP C++ wrapper and the Android UMP API.
 */
public class ConsentInfoHelper {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Synchronization object for thread safe access to mInternalPtr, mActivityRef, and mConsentForm.
  private final Object mLock = new Object();
  private Context mAppContext;
  private final Handler mMainHandler = new Handler(Looper.getMainLooper());

  // Pointer to the internal ConsentInfoInternalAndroid C++ object.
  // This can be reset back to 0 by calling disconnect().
  private long mInternalPtr = 0;
  // Weak reference to the latest Activity so configuration changes do not leak a destroyed Activity.
  private WeakReference<Activity> mActivityRef;
  // The loaded consent form, if any.
  private ConsentForm mConsentForm = null;

  // Create our own local passthrough version of these enum object values
  // as integers, to make it easier for the C++ SDK to access them.
  public static final int PRIVACY_OPTIONS_REQUIREMENT_UNKNOWN =
      PrivacyOptionsRequirementStatus.UNKNOWN.ordinal();
  public static final int PRIVACY_OPTIONS_REQUIREMENT_REQUIRED =
      PrivacyOptionsRequirementStatus.REQUIRED.ordinal();
  public static final int PRIVACY_OPTIONS_REQUIREMENT_NOT_REQUIRED =
      PrivacyOptionsRequirementStatus.NOT_REQUIRED.ordinal();

  // Enum values for tracking which function we are calling back.
  // Ensure these are incremental starting at 0.
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
        if (mAppContext == null) {
          mAppContext = activity.getApplicationContext();
        }
      }
    }
  }

  private static boolean isActivityUsable(Activity activity) {
    if (activity == null || activity.isFinishing()) {
      return false;
    }
    if (Build.VERSION.SDK_INT >= 17 && activity.isDestroyed()) {
      return false;
    }
    return true;
  }

  private Activity resolveActivityLocked(Activity explicitActivity) {
    if (isActivityUsable(explicitActivity)) {
      return explicitActivity;
    }
    Activity fallback = mActivityRef != null ? mActivityRef.get() : null;
    if (isActivityUsable(fallback)) {
      return fallback;
    }
    return null;
  }

  private Context getSafeContextLocked() {
    if (mAppContext != null) {
      return mAppContext;
    }
    return resolveActivityLocked(null);
  }

  private void completeWithErrorLocked(int functionId, long futureHandle, int code, String msg) {
    if (mInternalPtr != CPP_NULLPTR) {
      completeFuture(functionId, mInternalPtr, futureHandle, code, msg);
    }
  }

  public int getConsentStatus() {
    Context context;
    synchronized (mLock) {
      context = getSafeContextLocked();
    }
    if (context == null) {
      return ConsentInformation.ConsentStatus.UNKNOWN;
    }
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(context);
    return consentInfo.getConsentStatus();
  }

  public void requestConsentInfoUpdate(final long futureHandle, boolean tagForUnderAgeOfConsent,
      int debugGeography, ArrayList<String> debugIdList) {
    final int functionId = FUNCTION_REQUEST_CONSENT_INFO_UPDATE;
    final Activity targetActivity;
    final Context safeContext;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR) {
        return;
      }
      targetActivity = resolveActivityLocked(null);
      safeContext = getSafeContextLocked();
      if (targetActivity == null || safeContext == null) {
        completeWithErrorLocked(
            functionId,
            futureHandle,
            FormError.ErrorCode.INVALID_OPERATION,
            "Cannot request consent info update: Activity is null or destroyed.");
        return;
      }
    }

    ConsentDebugSettings.Builder debugSettingsBuilder = null;
    if (debugGeography != ConsentDebugSettings.DebugGeography.DEBUG_GEOGRAPHY_DISABLED) {
      debugSettingsBuilder =
          new ConsentDebugSettings.Builder(safeContext).setDebugGeography(debugGeography);
    }
    if (debugIdList != null && !debugIdList.isEmpty()) {
      if (debugSettingsBuilder == null) {
        debugSettingsBuilder = new ConsentDebugSettings.Builder(safeContext);
      }
      for (int i = 0; i < debugIdList.size(); i++) {
        String id = debugIdList.get(i);
        if (id != null) {
          debugSettingsBuilder = debugSettingsBuilder.addTestDeviceHashedId(id);
        }
      }
    }
    ConsentRequestParameters.Builder paramsBuilder =
        new ConsentRequestParameters.Builder().setTagForUnderAgeOfConsent(tagForUnderAgeOfConsent);
    if (debugSettingsBuilder != null) {
      paramsBuilder = paramsBuilder.setConsentDebugSettings(debugSettingsBuilder.build());
    }
    final ConsentRequestParameters params = paramsBuilder.build();

    mMainHandler.post(new Runnable() {
      @Override
      public void run() {
        try {
          Activity currentActivity;
          synchronized (mLock) {
            if (mInternalPtr == CPP_NULLPTR) {
              return;
            }
            currentActivity = resolveActivityLocked(targetActivity);
            if (currentActivity == null) {
              completeWithErrorLocked(
                  functionId,
                  futureHandle,
                  FormError.ErrorCode.INVALID_OPERATION,
                  "Activity was destroyed before requestConsentInfoUpdate could run.");
              return;
            }
          }
          ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(safeContext);
          consentInfo.requestConsentInfoUpdate(
              currentActivity,
              params,
              new OnConsentInfoUpdateSuccessListener() {
                @Override
                public void onConsentInfoUpdateSuccess() {
                  synchronized (mLock) {
                    completeWithErrorLocked(functionId, futureHandle, 0, null);
                  }
                }
              },
              new OnConsentInfoUpdateFailureListener() {
                @Override
                public void onConsentInfoUpdateFailure(FormError formError) {
                  synchronized (mLock) {
                    int code =
                        formError != null
                            ? formError.getErrorCode()
                            : FormError.ErrorCode.INTERNAL_ERROR;
                    String msg = formError != null ? formError.getMessage() : "Unknown error";
                    completeWithErrorLocked(functionId, futureHandle, code, msg);
                  }
                }
              });
        } catch (Throwable t) {
          synchronized (mLock) {
            completeWithErrorLocked(
                functionId, futureHandle, FormError.ErrorCode.INTERNAL_ERROR, t.getMessage());
          }
        }
      }
    });
  }

  public void loadConsentForm(final long futureHandle) {
    final int functionId = FUNCTION_LOAD_CONSENT_FORM;
    final Context safeContext;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR) {
        return;
      }
      safeContext = getSafeContextLocked();
      if (safeContext == null) {
        completeWithErrorLocked(
            functionId,
            futureHandle,
            FormError.ErrorCode.INVALID_OPERATION,
            "Cannot load consent form: Context is null.");
        return;
      }
    }

    mMainHandler.post(new Runnable() {
      @Override
      public void run() {
        try {
          Context currentContext;
          synchronized (mLock) {
            if (mInternalPtr == CPP_NULLPTR) {
              return;
            }
            currentContext = getSafeContextLocked();
            if (currentContext == null) {
              completeWithErrorLocked(
                  functionId,
                  futureHandle,
                  FormError.ErrorCode.INVALID_OPERATION,
                  "Context was destroyed before loadConsentForm could run.");
              return;
            }
          }
          UserMessagingPlatform.loadConsentForm(
              currentContext,
              new OnConsentFormLoadSuccessListener() {
                @Override
                public void onConsentFormLoadSuccess(ConsentForm form) {
                  synchronized (mLock) {
                    if (mInternalPtr == CPP_NULLPTR) {
                      return;
                    }
                    mConsentForm = form;
                    completeWithErrorLocked(functionId, futureHandle, 0, null);
                  }
                }
              },
              new OnConsentFormLoadFailureListener() {
                @Override
                public void onConsentFormLoadFailure(FormError formError) {
                  synchronized (mLock) {
                    if (mInternalPtr == CPP_NULLPTR) {
                      return;
                    }
                    mConsentForm = null;
                    int code =
                        formError != null
                            ? formError.getErrorCode()
                            : FormError.ErrorCode.INTERNAL_ERROR;
                    String msg = formError != null ? formError.getMessage() : "Unknown error";
                    completeWithErrorLocked(functionId, futureHandle, code, msg);
                  }
                }
              });
        } catch (Throwable t) {
          synchronized (mLock) {
            completeWithErrorLocked(
                functionId, futureHandle, FormError.ErrorCode.INTERNAL_ERROR, t.getMessage());
          }
        }
      }
    });
  }

  public boolean showConsentForm(final long futureHandle, final Activity activity) {
    final int functionId = FUNCTION_SHOW_CONSENT_FORM;
    final ConsentForm consentFormForThread;
    final Activity targetActivity;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR || mConsentForm == null) {
        return false;
      }
      targetActivity = resolveActivityLocked(activity);
      if (targetActivity == null) {
        // Do NOT consume mConsentForm when the Activity is null or destroyed.
        completeWithErrorLocked(
            functionId,
            futureHandle,
            FormError.ErrorCode.INVALID_OPERATION,
            "Cannot show consent form: Activity is null or destroyed.");
        return true;
      }
      consentFormForThread = mConsentForm;
      mConsentForm = null;
    }

    mMainHandler.post(new Runnable() {
      @Override
      public void run() {
        try {
          Activity currentActivity;
          synchronized (mLock) {
            if (mInternalPtr == CPP_NULLPTR) {
              return;
            }
            currentActivity = resolveActivityLocked(targetActivity);
            if (currentActivity == null) {
              if (mConsentForm == null) {
                mConsentForm = consentFormForThread;
              }
              completeWithErrorLocked(
                  functionId,
                  futureHandle,
                  FormError.ErrorCode.INVALID_OPERATION,
                  "Activity was destroyed before showConsentForm could run.");
              return;
            }
          }
          consentFormForThread.show(
              currentActivity,
              new OnConsentFormDismissedListener() {
                @Override
                public void onConsentFormDismissed(FormError formError) {
                  synchronized (mLock) {
                    if (formError == null) {
                      completeWithErrorLocked(functionId, futureHandle, 0, null);
                    } else {
                      completeWithErrorLocked(
                          functionId,
                          futureHandle,
                          formError.getErrorCode(),
                          formError.getMessage());
                    }
                  }
                }
              });
        } catch (Throwable t) {
          synchronized (mLock) {
            completeWithErrorLocked(
                functionId, futureHandle, FormError.ErrorCode.INTERNAL_ERROR, t.getMessage());
          }
        }
      }
    });
    return true;
  }

  public void loadAndShowConsentFormIfRequired(final long futureHandle, final Activity activity) {
    final int functionId = FUNCTION_LOAD_AND_SHOW_CONSENT_FORM_IF_REQUIRED;
    final Activity targetActivity;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR) {
        return;
      }
      targetActivity = resolveActivityLocked(activity);
      if (targetActivity == null) {
        completeWithErrorLocked(
            functionId,
            futureHandle,
            FormError.ErrorCode.INVALID_OPERATION,
            "Cannot load and show consent form: Activity is null or destroyed.");
        return;
      }
    }

    mMainHandler.post(new Runnable() {
      @Override
      public void run() {
        try {
          Activity currentActivity;
          synchronized (mLock) {
            if (mInternalPtr == CPP_NULLPTR) {
              return;
            }
            currentActivity = resolveActivityLocked(targetActivity);
            if (currentActivity == null) {
              completeWithErrorLocked(
                  functionId,
                  futureHandle,
                  FormError.ErrorCode.INVALID_OPERATION,
                  "Activity was destroyed before loadAndShowConsentFormIfRequired could run.");
              return;
            }
          }
          UserMessagingPlatform.loadAndShowConsentFormIfRequired(
              currentActivity,
              new OnConsentFormDismissedListener() {
                @Override
                public void onConsentFormDismissed(FormError formError) {
                  synchronized (mLock) {
                    if (formError == null) {
                      completeWithErrorLocked(functionId, futureHandle, 0, null);
                    } else {
                      completeWithErrorLocked(
                          functionId,
                          futureHandle,
                          formError.getErrorCode(),
                          formError.getMessage());
                    }
                  }
                }
              });
        } catch (Throwable t) {
          synchronized (mLock) {
            completeWithErrorLocked(
                functionId, futureHandle, FormError.ErrorCode.INTERNAL_ERROR, t.getMessage());
          }
        }
      }
    });
  }

  public int getPrivacyOptionsRequirementStatus() {
    Context context;
    synchronized (mLock) {
      context = getSafeContextLocked();
    }
    if (context == null) {
      return PRIVACY_OPTIONS_REQUIREMENT_UNKNOWN;
    }
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(context);
    return consentInfo.getPrivacyOptionsRequirementStatus().ordinal();
  }

  public void showPrivacyOptionsForm(final long futureHandle, final Activity activity) {
    final int functionId = FUNCTION_SHOW_PRIVACY_OPTIONS_FORM;
    final Activity targetActivity;
    synchronized (mLock) {
      if (mInternalPtr == CPP_NULLPTR) {
        return;
      }
      targetActivity = resolveActivityLocked(activity);
      if (targetActivity == null) {
        completeWithErrorLocked(
            functionId,
            futureHandle,
            FormError.ErrorCode.INVALID_OPERATION,
            "Cannot show privacy options form: Activity is null or destroyed.");
        return;
      }
    }

    mMainHandler.post(new Runnable() {
      @Override
      public void run() {
        try {
          Activity currentActivity;
          synchronized (mLock) {
            if (mInternalPtr == CPP_NULLPTR) {
              return;
            }
            currentActivity = resolveActivityLocked(targetActivity);
            if (currentActivity == null) {
              completeWithErrorLocked(
                  functionId,
                  futureHandle,
                  FormError.ErrorCode.INVALID_OPERATION,
                  "Activity was destroyed before showPrivacyOptionsForm could run.");
              return;
            }
          }
          UserMessagingPlatform.showPrivacyOptionsForm(
              currentActivity,
              new OnConsentFormDismissedListener() {
                @Override
                public void onConsentFormDismissed(FormError formError) {
                  synchronized (mLock) {
                    if (formError == null) {
                      completeWithErrorLocked(functionId, futureHandle, 0, null);
                    } else {
                      completeWithErrorLocked(
                          functionId,
                          futureHandle,
                          formError.getErrorCode(),
                          formError.getMessage());
                    }
                  }
                }
              });
        } catch (Throwable t) {
          synchronized (mLock) {
            completeWithErrorLocked(
                functionId, futureHandle, FormError.ErrorCode.INTERNAL_ERROR, t.getMessage());
          }
        }
      }
    });
  }

  public boolean canRequestAds() {
    Context context;
    synchronized (mLock) {
      context = getSafeContextLocked();
    }
    if (context == null) {
      return false;
    }
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(context);
    return consentInfo.canRequestAds();
  }

  public boolean isConsentFormAvailable() {
    Context context;
    synchronized (mLock) {
      context = getSafeContextLocked();
    }
    if (context == null) {
      return false;
    }
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(context);
    return consentInfo.isConsentFormAvailable();
  }

  public void reset() {
    Context context;
    synchronized (mLock) {
      mConsentForm = null;
      context = getSafeContextLocked();
    }
    if (context != null) {
      ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(context);
      consentInfo.reset();
    }
  }

  /** Disconnect the helper from the native object. */
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
