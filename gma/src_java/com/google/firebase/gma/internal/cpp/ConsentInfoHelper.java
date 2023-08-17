/*
 * Copyright 2021 Google LLC
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
import com.google.android.ump.ConsentInformation;
import com.google.android.ump.ConsentInformation.OnConsentInfoUpdateFailureListener;
import com.google.android.ump.ConsentInformation.OnConsentInfoUpdateSuccessListener;
import com.google.android.ump.ConsentRequestParameters;
import com.google.android.ump.ConsentDebugSettings;
import com.google.android.ump.FormError;
import com.google.android.ump.UserMessagingPlatform;
import java.util.ArrayList;

/**
 * Helper class to make interactions between the GMA UMP C++ wrapper and the Android UMP API.
 */
public class ConsentInfoHelper {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // Synchronization object for thread safe access to:
  private final Object mLock;

  private long mInternalPtr;

  private Activity mActivity;

  public ConsentInfoHelper(long consentInfoInternalPtr, Activity activity) {
    mInternalPtr = consentInfoInternalPtr;
    mLock = new Object();
    mActivity = activity;
    // Test the callbacks and fail quickly if something's wrong.
    completeRequestConsentInfoUpdate(CPP_NULLPTR, 0, "");
  }

  int getConsentStatus() {
    ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(mActivity);
    return consentInfo.getConsentStatus();
  }

  void requestConsentInfoUpdate(boolean tagForUnderAgeOfConsent,
				int debugGeography, ArrayList<String> debugIdList) {
    ConsentDebugSettings.Builder debugSettingsBuilder = null;
    
    if (debugGeography != ConsentDebugSettings.DebugGeography.DEBUG_GEOGRAPHY_DISABLED) {
	debugSettingsBuilder = new ConsentDebugSettings
	    .Builder(mActivity)
	    .setDebugGeography(debugGeography);
    }
    if (debugIdList != null && debugIdList.size() > 0) {
	if (debugSettingsBuilder == null) {
	    debugSettingsBuilder = new ConsentDebugSettings.Builder(mActivity);
	}
	for (int i=0; i < debugIdList.size(); i++) {
	    debugSettingsBuilder = debugSettingsBuilder.addTestDeviceHashedId(debugIdList.get(i));
	}
    }
    ConsentRequestParameters.Builder paramsBuilder = new ConsentRequestParameters
	.Builder()
	.setTagForUnderAgeOfConsent(tagForUnderAgeOfConsent);

    if (debugSettingsBuilder != null) {
	paramsBuilder = paramsBuilder.setConsentDebugSettings(debugSettingsBuilder.build());
    }

    final ConsentRequestParameters params = paramsBuilder.build();

    mActivity.runOnUiThread(new Runnable() {
      @Override
      public void run() {

	  ConsentInformation consentInfo = UserMessagingPlatform.getConsentInformation(mActivity);
	  consentInfo.requestConsentInfoUpdate
	      (mActivity,
	       params,
	       new OnConsentInfoUpdateSuccessListener() {
		   @Override
		   public void onConsentInfoUpdateSuccess() {
		       synchronized (mLock) {
			   completeRequestConsentInfoUpdate(mInternalPtr, 0, "");
		       }
		   }
	       },
	       new OnConsentInfoUpdateFailureListener() {
		   @Override
		   public void onConsentInfoUpdateFailure(FormError requestConsentError) {
		       synchronized (mLock) {
			   completeRequestConsentInfoUpdate(mInternalPtr,
							    requestConsentError.getErrorCode(),
							    requestConsentError.getMessage());
		       }
		   }
	       });
      }
	});
  }
  

  /** Disconnect the helper from the native object. */
  public void disconnect() {
    synchronized (mLock) {
      mInternalPtr = CPP_NULLPTR;
    }
  }

  public static native void completeRequestConsentInfoUpdate(
      long nativeInternalPtr, int errorCode, String errorMessage);

  public static native void completeLoadConsentForm(
      long nativeInternalPtr, int errorCode, String errorMessage);
  
  public static native void completeShowConsentForm(
      long nativeInternalPtr, int errorCode, String errorMessage);
  
  public static native void completeLoadAndShowIfRequired(
      long nativeInternalPtr, int errorCode, String errorMessage);
  
  public static native void completeShowPrivacyOptionsForm(
      long nativeInternalPtr, int errorCode, String errorMessage);
}
