/*
 * Copyright 2017 Google LLC
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

// This file contains a wrapper class used by the App Invites C++ API.
// See go/firebase-app-invites-cpp for more information.

package com.google.firebase.dynamiclinks.internal.cpp;

import android.app.Activity;
import android.net.Uri;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.firebase.app.internal.cpp.Log;
import com.google.firebase.dynamiclinks.FirebaseDynamicLinks;
import com.google.firebase.dynamiclinks.PendingDynamicLinkData;

/**
 * Wrapper class used by the C++ implementation of Dynamic Links. This class wraps the Dynamic Links
 * API in a way that can be easily used by the C++ JNI implementation. It handles talking to the
 * standard Dynamic Links library and handles callbacks back to the C++ code.
 *
 * @hide
 */
public class DynamicLinksNativeWrapper {
  private static final String TAG = "DynamicLinksNW";
  private Activity activity;
  private long nativeInternalPtr;

  // Message that describes the failure to find a native method and a potential resolution.
  private static final String NATIVE_METHOD_MISSING_MESSAGE =
      "Native method not found, unable to send %s message to the application. "
          + "Make sure Firebase was not shut down while a Dynamic Links fetch operation "
          + "was in progress. (%s)";

  public DynamicLinksNativeWrapper(long nativePtr, Activity incomingActivity) {

    nativeInternalPtr = nativePtr;
    // Call our JNI function (passing in 0 as the nativeInternalPtr) so we can verify that
    // they are all hooked up properly.
    //
    // On the C++ side, they will no-op if nativeInternalPtr is 0, so all we are doing is checking
    // that they are registered / named properly. If any of them are not, they will throw an
    // UnsatisfiedLinkError exception that needs to be caught by whatever is constructing us.
    // NOTE: It's fine to use the unsafe native methods here as the constructor of this object is
    // being called from C++.
    receivedDynamicLinkCallback(0, null, 0, null);
    activity = incomingActivity;
  }

  public void discardNativePointer() {
    nativeInternalPtr = 0;
  }

  // Invitation receiving:

  // Fetch Dynamic Links - this function is called from JNI to trigger checking for dynamic links.
  public boolean fetchDynamicLink() {
    try {
      activity.runOnUiThread(
          new Runnable() {
            @Override
            public void run() {
              FirebaseDynamicLinks.getInstance()
                  .getDynamicLink(activity.getIntent())
                  .addOnSuccessListener(
                      activity,
                      new OnSuccessListener<PendingDynamicLinkData>() {
                        @Override
                        public void onSuccess(PendingDynamicLinkData pendingDynamicLinkData) {
                          // Get deep link from result (may be null if no link is found)
                          Uri deepLink = null;
                          String deepLinkString = null;
                          if (pendingDynamicLinkData != null) {
                            deepLink = pendingDynamicLinkData.getLink();
                            if (deepLink != null) {
                              deepLinkString = deepLink.toString();
                            }
                          }
                          safeReceivedDynamicLinkCallback(nativeInternalPtr, deepLinkString, 0, "");
                        }
                      })
                  .addOnFailureListener(
                      activity,
                      new OnFailureListener() {
                        @Override
                        public void onFailure(Exception e) {
                          safeReceivedDynamicLinkCallback(
                              nativeInternalPtr, "", Activity.RESULT_CANCELED, e.toString());
                        }
                      });
            }
          });
      return true;
    } catch (Exception e) {
      // Failed to start fetching invites. The client code will handle this when we return false..
      return false;
    }
  }

  public static void safeReceivedDynamicLinkCallback(
      long nativeInternalPtr, String deepLinkUrl, int resultCode, String errorString) {
    try {
      receivedDynamicLinkCallback(nativeInternalPtr, deepLinkUrl, resultCode, errorString);
    } catch (UnsatisfiedLinkError e) {
      String missingDetails =
          String.format(
              "received url=%s (code=%d, message=%s)",
              String.valueOf(deepLinkUrl), resultCode, errorString);
      Log.e(TAG, String.format(NATIVE_METHOD_MISSING_MESSAGE, missingDetails, e.toString()));
    }
  }

  public static native void receivedDynamicLinkCallback(
      long nativeInternalPtr, String deepLinkUrl, int resultCode, String errorString);
}
