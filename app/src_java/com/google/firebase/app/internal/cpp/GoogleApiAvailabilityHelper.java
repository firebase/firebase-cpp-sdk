/*
 * Copyright 2016 Google LLC
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

package com.google.firebase.app.internal.cpp;


import android.app.Activity;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.tasks.OnCanceledListener;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;

/** Use GoogleApiAvailability to check whether Google Play Services is available and up-to-date. */
public class GoogleApiAvailabilityHelper {
  private static final String TAG = "GMSAvailability";

  /** Succeeded in making Play available. */
  private static final int SUCCESS = 0;
  /** Failed due to user canceling the dialog box. */
  private static final int FAILURE_CANCELED = 1;
  /** Could not check the status of the update (couldn't check device task list). */
  private static final int FAILURE_COULD_NOT_CHECK = 2;
  /** Failed to update, the status stayed invalid after returning to the app. */
  private static final int FAILURE_COULD_NOT_UPDATE = 3;
  /** Timed out while updating. */
  private static final int FAILURE_TIMEOUT = 4;

  private static final int REQUEST_CODE = -1; // Negative so no result is sent.
  private static Activity currentActivity = null;
  private static boolean stopAllCallbacks = false;
  private static Object lock = new Object(); // Lock for stopAllCallbacks and currentActivity.

  /**
   * If GmsCore is unavailable but can be fixed, show a dialog box to prompt the user to fix it. You
   * must call this from the UI thread.
   */
  public static boolean makeGooglePlayServicesAvailable(Activity activity) {
    synchronized (lock) {
      if (currentActivity != null) {
        // Only allow one to be running at a time.
        return false;
      }
      currentActivity = activity;
      stopAllCallbacks = false;
    }
    GoogleApiAvailability api = GoogleApiAvailability.getInstance();
    final int errorCode = api.isGooglePlayServicesAvailable(currentActivity);
    if (errorCode == ConnectionResult.SUCCESS) {
      // Google Play Services is already available.
      synchronized (lock) {
        currentActivity = null;
      }
      GoogleApiAvailabilityHelper.onComplete(SUCCESS, "Google Play services are already available");
      return true;
    } else if (!api.isUserResolvableError(errorCode)) {
      // We can't make Google Play services available, as the error is not resolvable.
      synchronized (lock) {
        currentActivity = null;
      }
      Log.e(TAG, "Unable to make Google Play services available, failed with error " + errorCode);
      return false;
    } else {
      Task task = api.makeGooglePlayServicesAvailable(activity);
      if (task == null) {
        return false;
      }
      task.addOnSuccessListener(
          new OnSuccessListener<Void>() {
            @Override
            public void onSuccess(Void result) {
              GoogleApiAvailabilityHelper.onComplete(SUCCESS, "Success");
            }
          });
      task.addOnFailureListener(
          new OnFailureListener() {
            @Override
            public void onFailure(Exception exception) {
              GoogleApiAvailabilityHelper.onComplete(
                  FAILURE_COULD_NOT_UPDATE,
                  "Couldn't make Google Play services available: " + exception.getMessage());
            }
          });
      task.addOnCanceledListener(
          new OnCanceledListener() {
            @Override
            public void onCanceled() {
              GoogleApiAvailabilityHelper.onComplete(FAILURE_CANCELED, "User canceled the update");
            }
          });
      return true;
    }
  }

  /** Tell makeGooglePlayServicesAvailable to cease calling any further native callbacks. */
  public static void stopCallbacks() {
    synchronized (lock) {
      stopAllCallbacks = true;
    }
  }

  private static void onComplete(int resultCode, String resultMessage) {
    synchronized (lock) {
      if (!stopAllCallbacks) {
        onCompleteNative(resultCode, resultMessage);
      }
      currentActivity = null;
    }
  }

  private static native void onCompleteNative(int resultCode, String resultMessage);
}
