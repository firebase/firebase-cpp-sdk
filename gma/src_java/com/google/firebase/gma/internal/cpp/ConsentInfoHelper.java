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

package com.google.firebase.gma.internal.cpp;

import android.app.Activity;
import android.content.Context;
import android.util.Log;
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
import java.util.ArrayList;

/**
 * Helper class to make interactions between the GMA UMP C++ wrapper and the Android UMP API.
 */
public class ConsentInfoHelper {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Synchronization object for thread safe access to:
  private final Object mLock = new Object();
  // Pointer to the internal ConsentInfoInternalAndroid C++ object.
  // This can be reset back to 0 by calling disconnect().
  private long mInternalPtr = 0;
  // The Activity that this was initialized with.
  private Activity mActivity = null;
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
  // These don't have to match ConsentInfoFn, as the C++ code will
  // use these Java enums directly.
  public static final int FUNCTION_REQUEST_CONSENT_INFO_UPDATE = 0;
  public static final int FUNCTION_LOAD_CONSENT_FORM = 1;
  public static final int FUNCTION_SHOW_CONSENT_FORM = 2;
  public static final int FUNCTION_LOAD_AND_SHOW_CONSENT_FORM_IF_REQUIRED = 3;
  public static final int FUNCTION_SHOW_PRIVACY_OPTIONS_FORM = 4;
  public static final int FUNCTION_COUNT = 5;

  public ConsentInfoHelper(long consentInfoInternalPtr, Activity activity) {
    synchronized (mLock) {
      mInternalPtr = consentInfoInternalPtr;
      mActivity = activity;
      // Test the callbacks and fail quickly if something's wrong.
      completeFuture(-1, CPP_NULLPTR, CPP_NULLPTR, 0, null);
    }
  }

  public int getConsentStatus() {
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(mActivity);
    return consentInfo.getConsentStatus();
  }

  public void requestConsentInfoUpdate(final long futureHandle, boolean tagForUnderAgeOfConsent,
      int debugGeography, ArrayList<String> debugIdList) {
    synchronized (mLock) {
      if (mInternalPtr == 0)
        return;
    }
    final int functionId = FUNCTION_REQUEST_CONSENT_INFO_UPDATE;

    ConsentDebugSettings.Builder debugSettingsBuilder = null;

    // Only create and use debugSettingsBuilder if a debug option is set.
    if (debugGeography != ConsentDebugSettings.DebugGeography.DEBUG_GEOGRAPHY_DISABLED) {
      debugSettingsBuilder =
          new ConsentDebugSettings.Builder(mActivity).setDebugGeography(debugGeography);
    }
    if (debugIdList != null && debugIdList.size() > 0) {
      if (debugSettingsBuilder == null) {
        debugSettingsBuilder = new ConsentDebugSettings.Builder(mActivity);
      }
      for (int i = 0; i < debugIdList.size(); i++) {
        debugSettingsBuilder = debugSettingsBuilder.addTestDeviceHashedId(debugIdList.get(i));
      }
    }
    ConsentRequestParameters.Builder paramsBuilder =
        new ConsentRequestParameters.Builder().setTagForUnderAgeOfConsent(tagForUnderAgeOfConsent);

    if (debugSettingsBuilder != null) {
      paramsBuilder = paramsBuilder.setConsentDebugSettings(debugSettingsBuilder.build());
    }

    final ConsentRequestParameters params = paramsBuilder.build();

    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(mActivity);
        consentInfo.requestConsentInfoUpdate(mActivity, params,
            new OnConsentInfoUpdateSuccessListener() {
              @Override
              public void onConsentInfoUpdateSuccess() {
                synchronized (mLock) {
                  if (mInternalPtr == 0)
                    return;
                  completeFuture(functionId, mInternalPtr, futureHandle, 0, null);
                }
              }
            },
            new OnConsentInfoUpdateFailureListener() {
              @Override
              public void onConsentInfoUpdateFailure(FormError formError) {
                synchronized (mLock) {
                  if (mInternalPtr == 0)
                    return;
                  completeFuture(functionId, mInternalPtr, futureHandle, formError.getErrorCode(),
                      formError.getMessage());
                }
              }
            });
      }
    });
  }

  public void loadConsentForm(final long futureHandle) {
    synchronized (mLock) {
      if (mInternalPtr == 0)
        return;
    }
    final int functionId = FUNCTION_LOAD_CONSENT_FORM;
    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        UserMessagingPlatform.loadConsentForm(mActivity,
            new OnConsentFormLoadSuccessListener() {
              @Override
              public void onConsentFormLoadSuccess(ConsentForm form) {
                synchronized (mLock) {
                  if (mInternalPtr == 0)
                    return;
                  mConsentForm = form;
                  completeFuture(functionId, mInternalPtr, futureHandle, 0, null);
                }
              }
            },
            new OnConsentFormLoadFailureListener() {
              @Override
              public void onConsentFormLoadFailure(FormError formError) {
                synchronized (mLock) {
                  if (mInternalPtr == 0)
                    return;
                  mConsentForm = null;
                  completeFuture(functionId, mInternalPtr, futureHandle, formError.getErrorCode(),
                      formError.getMessage());
                }
              }
            });
      }
    });
  }

  public boolean showConsentForm(final long futureHandle, final Activity activity) {
    synchronized (mLock) {
      if (mInternalPtr == 0)
        return false;
    }
    final int functionId = FUNCTION_SHOW_CONSENT_FORM;
    ConsentForm consentForm;
    synchronized (mLock) {
      if (mConsentForm == null) {
        // Consent form was not loaded, return an error.
        return false;
      }
      consentForm = mConsentForm;
      mConsentForm = null;
    }
    final ConsentForm consentFormForThread = consentForm;
    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        consentFormForThread.show(activity, new OnConsentFormDismissedListener() {
          @Override
          public void onConsentFormDismissed(FormError formError) {
            synchronized (mLock) {
              if (mInternalPtr == 0)
                return;
              if (formError == null) {
                completeFuture(functionId, mInternalPtr, futureHandle, 0, null);
              } else {
                completeFuture(functionId, mInternalPtr, futureHandle, formError.getErrorCode(),
                    formError.getMessage());
              }
            }
          }
        });
      }
    });
    // Consent form is loaded.
    return true;
  }

  public void loadAndShowConsentFormIfRequired(final long futureHandle, final Activity activity) {
    synchronized (mLock) {
      if (mInternalPtr == 0)
        return;
    }
    final int functionId = FUNCTION_LOAD_AND_SHOW_CONSENT_FORM_IF_REQUIRED;
    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        UserMessagingPlatform.loadAndShowConsentFormIfRequired(
            activity, new OnConsentFormDismissedListener() {
              @Override
              public void onConsentFormDismissed(FormError formError) {
                synchronized (mLock) {
                  if (mInternalPtr == 0)
                    return;
                  if (formError == null) {
                    completeFuture(functionId, mInternalPtr, futureHandle, 0, null);
                  } else {
                    completeFuture(functionId, mInternalPtr, futureHandle, formError.getErrorCode(),
                        formError.getMessage());
                  }
                }
              }
            });
      }
    });
  }

  public int getPrivacyOptionsRequirementStatus() {
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(mActivity);
    return consentInfo.getPrivacyOptionsRequirementStatus().ordinal();
  }

  public void showPrivacyOptionsForm(final long futureHandle, final Activity activity) {
    synchronized (mLock) {
      if (mInternalPtr == 0)
        return;
    }
    final int functionId = FUNCTION_SHOW_PRIVACY_OPTIONS_FORM;
    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        UserMessagingPlatform.showPrivacyOptionsForm(
            activity, new OnConsentFormDismissedListener() {
              @Override
              public void onConsentFormDismissed(FormError formError) {
                synchronized (mLock) {
                  if (mInternalPtr == 0)
                    return;
                  if (formError == null) {
                    completeFuture(functionId, mInternalPtr, futureHandle, 0, null);
                  } else {
                    completeFuture(functionId, mInternalPtr, futureHandle, formError.getErrorCode(),
                        formError.getMessage());
                  }
                }
              }
            });
      }
    });
  }

  public boolean canRequestAds() {
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(mActivity);
    return consentInfo.canRequestAds();
  }

  public boolean isConsentFormAvailable() {
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(mActivity);
    return consentInfo.isConsentFormAvailable();
  }

  public void reset() {
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(mActivity);
    consentInfo.reset();
  }

  /** Disconnect the helper from the native object. */
  public void disconnect() {
    synchronized (mLock) {
      mInternalPtr = CPP_NULLPTR;
    }
  }
  public static native void completeFuture(
      int futureFn, long nativeInternalPtr, long futureHandle, int errorCode, String errorMessage);
}
