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

package com.google.firebase.invites.internal.cpp;

import android.app.Activity;
import android.app.Fragment;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import com.google.android.gms.appinvite.AppInvite;
import com.google.android.gms.appinvite.AppInviteInvitation;
import com.google.android.gms.appinvite.AppInviteInvitationResult;
import com.google.android.gms.appinvite.AppInviteReferral;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;
import com.google.firebase.app.internal.cpp.Log;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * Wrapper class used by the C++ implementation of App Invites. This class wraps the Invites
 * API in a way that can be easily used by the C++ JNI implementation. It handles talking to
 * the standard AppInvites library, handles callbacks back to the C++ code, and takes care
 * of various lifecycle things like displaying the AppInviteInvitation intent and receiving
 * the results.
 *
 * @hide
 */
public class AppInviteNativeWrapper
    implements GoogleApiClient.OnConnectionFailedListener, GoogleApiClient.ConnectionCallbacks {
  private static final String TAG = "InvitesClass";

  /** Called when the client is temporarily disconnected. GoogleApiClient will automatically try to
   *  reconnect, so we don't need to do anything special here. (In a front-end app, you could notify
   *  the user they are temporarily disconnected, disable UI elements, etc.)
   */
  @Override
  public void onConnectionSuspended(int cause) {}

  /**
   * A subclass of Fragment, which we use to launch the AppInvites activity so that we can get the
   * activity result.
   */
  public static class NativeWrapperFragment extends Fragment {
    private AppInviteNativeWrapper nativeWrapper;
    private Intent intent;

    public void setIntentToLaunch(Intent intentToLaunch) {
      intent = intentToLaunch;
    }

    public void setAppInvitesWrapper(AppInviteNativeWrapper wrapper) {
      nativeWrapper = wrapper;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      if (intent != null) {
        startActivityForResult(intent, REQUEST_INVITE);
      }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
      super.onActivityResult(requestCode, resultCode, data);
      if (requestCode == REQUEST_INVITE) {
        String[] ids = AppInviteInvitation.getInvitationIds(resultCode, data);
        if (nativeWrapper != null) {
          nativeWrapper.gotInvitationResult(ids, resultCode, null);
        }
      }
    }
  }

  /** If there is an exception thrown when creating the invite sender UI, this will be the errorCode
   * returned via sentInviteCallback.
   */
  public static final int INVITE_SENDER_UI_ERROR = -100119;
  // Arbitrary requestCode number that we hope the given activity ignores.
  private static final int REQUEST_INVITE = 0x52494e56; // hex for "RINV"

  private static GoogleApiClient googleApiClient = null;
  private static Object googleApiLock = new Object();

  // A list of instances waiting for onConnected() to be called.
  private static ArrayList<AppInviteNativeWrapper> connectionWaiters = null;
  private Activity activity;
  private long nativeInternalPtr;

  // Invites doesn't work if you actually pass in a null deep link.
  // TODO(jsimantov): Remove this when App Invites in GMSCore is fixed, b/27612427
  private static final String NULL_DEEP_LINK_URL = "file:///dev/null";

  // Message that describes the failure to find a native method and a potential resolution.
  private static final String NATIVE_METHOD_MISSING_MESSAGE =
      "Native method not found, unable to send %s message to the application. "
      + "Make sure Firebase was not shut down while a Dynamic Links or Invites fetch operation "
      + "was in progress. (%s)";

  public AppInviteNativeWrapper(
      long nativePtr, Activity incomingActivity, GoogleApiClient apiClient) {
    if (connectionWaiters == null) {
      connectionWaiters = new ArrayList<AppInviteNativeWrapper>();
    }

    nativeInternalPtr = nativePtr;
    // Call all of our JNI functions (passing in 0 as the nativeInternalPtr) so we can verify that
    // they are all hooked up properly.
    //
    // On the C++ side, they will no-op if nativeInternalPtr is 0, so all we are doing is checking
    // that they are registered / named properly. If any of them are not, they will throw an
    // UnsatisfiedLinkError exception that needs to be caught by whatever is constructing us.
    // NOTE: It's fine to use the unsafe native methods here as the constructor of this object is
    // being called from C++.
    safeSentInviteCallback(0, null, 0, null);
    connectionFailedCallback(0, 0, null);
    receivedInviteCallback(0, null, null, 0, null);
    convertedInviteCallback(0, null, 0, null);

    activity = incomingActivity;
    invitationOptions = new HashMap<String, String>();

    synchronized (googleApiLock) {
      if (apiClient != null) {
        googleApiClient = apiClient; // Make sure the existing API has AppInvite in it!
      } else {
        if (googleApiClient == null || !googleApiClient.isConnected()) {
          connectionWaiters.add(this);
        }
        if (googleApiClient == null) {
          activity.runOnUiThread(
              new Runnable() {
                @Override
                public void run() {
                  synchronized (googleApiLock) {
                    if (googleApiClient == null) {
                      googleApiClient =
                          new GoogleApiClient.Builder(activity).addApi(AppInvite.API).build();
                      googleApiClient.registerConnectionCallbacks(AppInviteNativeWrapper.this);
                      googleApiClient.connect();
                    }
                  }
                }
              });
        }
      }
    }
    resetSenderSettings();
  }

  // Call this before the native class we refer to is deleted, or else we'll be pointing
  // to invalid memory. All JNI callbacks will ignore a nativeInternalPtr of 0.
  public void discardNativePointer() {
    nativeInternalPtr = 0;
    synchronized (googleApiLock) {
      connectionWaiters.clear();
    }
  }

  @Override
  public void onConnectionFailed(ConnectionResult connectionResult) {
    /* do something with the error...? */
    safeConnectionFailedCallback(
        nativeInternalPtr, connectionResult.getErrorCode(), connectionResult.getErrorMessage());
  }

  // Important:
  // These strings must be kept in sync with the strings used
  // in the invites_sender_android_internal C++ source file,
  // in the kJNIMapping array.
  // If you modify these strings or add additional ones, you must
  // change them there as well.
  public static final String OPTION_TITLE = "title";
  public static final String OPTION_MESSAGE = "message";
  public static final String OPTION_CUSTOM_IMAGE = "customImage";
  public static final String OPTION_CALL_TO_ACTION_TEXT = "callToActionText";
  public static final String OPTION_EMAIL_HTML_CONTENT = "emailHtmlContent";
  public static final String OPTION_EMAIL_SUBJECT = "emailSubject";
  public static final String OPTION_DEEP_LINK = "deepLink";
  public static final String OPTION_GOOGLE_ANALYTICS_TRACKING_ID = "googleAnalyticsTrackingId";
  public static final String OPTION_ANDROID_MINIMUM_VERSION_CODE = "androidMinimumVersionCode";
  public static final String OPTION_OTHER_PLATFORMS_TARGET_APPLICATION_IOS =
      "otherPlatformsTargetApplicationIOS";
  public static final String OPTION_OTHER_PLATFORMS_TARGET_APPLICATION_ANDROID =
      "otherPlatformsTargetApplicationAndroid";

  private HashMap<String, Integer> invitationOptionMaxLengths =
      new HashMap<String, Integer>() {
        {
          put(
              OPTION_CALL_TO_ACTION_TEXT,
              AppInviteInvitation.IntentBuilder.MAX_CALL_TO_ACTION_TEXT_LENGTH);
          put(OPTION_EMAIL_HTML_CONTENT, AppInviteInvitation.IntentBuilder.MAX_EMAIL_HTML_CONTENT);
          put(OPTION_MESSAGE, AppInviteInvitation.IntentBuilder.MAX_MESSAGE_LENGTH);
        }
      };

  private HashMap<String, Integer> invitationOptionMinLengths =
      new HashMap<String, Integer>() {
        {
          put(
              OPTION_CALL_TO_ACTION_TEXT,
              AppInviteInvitation.IntentBuilder.MIN_CALL_TO_ACTION_TEXT_LENGTH);
        }
      };

  // Contains the various options for an invitation we are going to send.
  private HashMap<String, String> invitationOptions;
  // Contains the "additional referral parameters" that the user can set on an invitation.
  private HashMap<String, String> additionalReferralParameters;

  // Clear invitation options and additional referral parameters.
  public void resetSenderSettings() {
    clearInvitationOptions();
    clearReferralParams();
  }

  // Clear just the invitation options.
  public void clearInvitationOptions() {
    invitationOptions.clear();
  }

  // Get the maximum allowed length of the given invitation option, or -1 for effectively unlimited.
  // These will be used for testing to ensure that constants have not deviated between Java and C++.
  public int getInvitationOptionMaxLength(String key) {
    String lKey = key.toLowerCase();
    if (invitationOptionMaxLengths.containsKey(lKey)) {
      return invitationOptionMaxLengths.get(lKey);
    }
    return -1;
  }

  // Get the minimum allowed length of the given invitation option.
  // These will be used for testing to ensure that constants have not deviated between Java and C++.
  public int getInvitationOptionMinLength(String key) {
    String lKey = key.toLowerCase();
    if (invitationOptionMinLengths.containsKey(lKey)) {
      return invitationOptionMinLengths.get(lKey);
    }
    return 0;
  }

  // Get a invitation option value, by case-insensitive key.
  public String getInvitationOption(String key) {
    if (hasInvitationOption(key)) {
      return invitationOptions.get(key.toLowerCase());
    }
    return null;
  }

  // Check if an invitation option exists.
  public boolean hasInvitationOption(String key) {
    return (invitationOptions.containsKey(key.toLowerCase()));
  }

  // Set the value for an invitation option. The key names match the names in the invites library.
  public void setInvitationOption(String key, String value) {
    if (value == null) {
      invitationOptions.remove(key.toLowerCase());
    } else {
      invitationOptions.put(key.toLowerCase(), value);
    }
  }

  // Add an additional referral parameter to the invitation URL.
  public void addReferralParam(String key, String value) {
    if (additionalReferralParameters == null) {
      additionalReferralParameters = new HashMap<String, String>();
    }
    if (value == null) {
      additionalReferralParameters.remove(key);
    } else {
      additionalReferralParameters.put(key, value);
    }
  }

  // Clear the referral parameters. If additionalReferralParameters is null,
  // setAdditionReferralParameters won't even be called on the invitation builder.
  public void clearReferralParams() {
    additionalReferralParameters = null;
  }

  NativeWrapperFragment fragment;

  // Display the app invites sending UI asynchronously. Returns true if the UI showed
  // properly, or false if it didn't. If anything happens while setting up the invites
  // UI, the sent invite callback will be called with a special error code given above
  // (INVITE_SENDER_UI_ERROR) which the native side will need to check for.
  public boolean showSenderUI() {
    // Title and either message or HTML message are always required. Without them, we fail to create
    // the invitation.
    if (!hasInvitationOption(OPTION_TITLE)
        || !(hasInvitationOption(OPTION_MESSAGE)
            || hasInvitationOption(OPTION_EMAIL_HTML_CONTENT))) {
      return false;
    }
    try {
      activity.runOnUiThread(
          new Runnable() {
            @Override
            public void run() {
              try {
                // Create the invite builder and set the relevant options.

                String title = getInvitationOption(OPTION_TITLE);
                String message = getInvitationOption(OPTION_MESSAGE);
                String customImage = getInvitationOption(OPTION_CUSTOM_IMAGE);
                String callToActionText = getInvitationOption(OPTION_CALL_TO_ACTION_TEXT);
                String emailHtmlContent = getInvitationOption(OPTION_EMAIL_HTML_CONTENT);
                String emailSubject = getInvitationOption(OPTION_EMAIL_SUBJECT);
                String deepLink = getInvitationOption(OPTION_DEEP_LINK);
                String googleAnalyticsTrackingId =
                    getInvitationOption(OPTION_GOOGLE_ANALYTICS_TRACKING_ID);
                String androidMinimumVersionCode =
                    getInvitationOption(OPTION_ANDROID_MINIMUM_VERSION_CODE);
                String otherPlatformsTargetApplicationIOS =
                    getInvitationOption(OPTION_OTHER_PLATFORMS_TARGET_APPLICATION_IOS);
                String otherPlatformsTargetApplicationAndroid =
                    getInvitationOption(OPTION_OTHER_PLATFORMS_TARGET_APPLICATION_ANDROID);

                AppInviteInvitation.IntentBuilder builder =
                    new AppInviteInvitation.IntentBuilder(title);

                if (emailHtmlContent != null && emailSubject != null) {
                  // The user provided HTML content, which takes precedence.
                  builder.setEmailHtmlContent(emailHtmlContent);
                  builder.setEmailSubject(emailSubject);
                } else {
                  // No HTML content, use the text content, if any.
                  if (message != null) {
                    builder.setMessage(message);
                  }
                  if (customImage != null) {
                    builder.setCustomImage(Uri.parse(customImage));
                  }
                  if (callToActionText != null) {
                    builder.setCallToActionText(callToActionText);
                  }
                }
                if (deepLink != null) {
                  builder.setDeepLink(Uri.parse(deepLink));
                } else {
                  // Bug workaround: not setting a deep link causes things to explode.
                  // TODO(jsimantov): Remove this when App Invites in GMSCore is fixed, b/27612427
                  builder.setDeepLink(Uri.parse(NULL_DEEP_LINK_URL));
                }
                if (googleAnalyticsTrackingId != null) {
                  builder.setGoogleAnalyticsTrackingId(googleAnalyticsTrackingId);
                }

                int androidMinimumVersionCodeNum;
                if (androidMinimumVersionCode != null) {
                  try {
                    androidMinimumVersionCodeNum = Integer.parseInt(androidMinimumVersionCode);
                  } catch (NumberFormatException e) {
                    androidMinimumVersionCodeNum = 0;
                  }
                  if (androidMinimumVersionCodeNum > 0) {
                    builder.setAndroidMinimumVersionCode(androidMinimumVersionCodeNum);
                  }
                }

                if (otherPlatformsTargetApplicationIOS != null) {
                  builder.setOtherPlatformsTargetApplication(
                      AppInviteInvitation.IntentBuilder.PlatformMode.PROJECT_PLATFORM_IOS,
                      otherPlatformsTargetApplicationIOS);
                }
                if (otherPlatformsTargetApplicationAndroid != null) {
                  builder.setOtherPlatformsTargetApplication(
                      AppInviteInvitation.IntentBuilder.PlatformMode.PROJECT_PLATFORM_ANDROID,
                      otherPlatformsTargetApplicationAndroid);
                }

                if (additionalReferralParameters != null) {
                  builder.setAdditionalReferralParameters(additionalReferralParameters);
                }
                Intent inviteIntent = builder.build();

                fragment = new NativeWrapperFragment();
                fragment.setIntentToLaunch(inviteIntent);
                fragment.setAppInvitesWrapper(AppInviteNativeWrapper.this);
                activity
                    .getFragmentManager()
                    .beginTransaction()
                    .add(fragment, "NativeWrapperFragment")
                    .commit();
              } catch (Exception e) {
                // If anything went wrong, then just give up, and pass in a special error code.
                safeSentInviteCallback(nativeInternalPtr, null, INVITE_SENDER_UI_ERROR,
                    e.getMessage());
              }
            }
          });
      return true;
    } catch (Exception e) {
      // Failed to show invite sending UI.
      return false;
    }
  }

  // Pass the invitation results back to the native code.
  public void gotInvitationResult(String[] invitationIDs, int resultCode, String errorString) {
    // If resultCode is normal (OK or CANCELED), errorCode is 0, indicating no error.
    // Otherwise pass through the resultCode.
    int errorCode =
        (resultCode == Activity.RESULT_OK || resultCode == Activity.RESULT_CANCELED)
            ? 0
            : resultCode;
    safeSentInviteCallback(nativeInternalPtr, invitationIDs, errorCode, errorString);
    activity.getFragmentManager().beginTransaction().remove(fragment).commit();
    fragment = null;
  }

  // Invitation receiving:

  // Set to true if the user tries to fetch invites before we are signed in.
  boolean waitingToFetchInvites = false;

  @Override
  public void onConnected(Bundle bundle) {
    activity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            finishPendingFetchInvite();
          }
        });
    ArrayList<AppInviteNativeWrapper> callList = null;
    synchronized (googleApiLock) {
      // Check if other AppInviteNativeWrapper instances are waiting to be notified we connected.
      if (connectionWaiters.size() > 0) {
        callList = new ArrayList<AppInviteNativeWrapper>(connectionWaiters);
        connectionWaiters.clear();
      }
    }
    if (callList != null) {
      for (AppInviteNativeWrapper wrapper : callList) {
        if (wrapper != this) {
          wrapper.onConnected(bundle);
        }
      }
    }
  }


  // Finish fetching invites after sign in. You should ensure this is run from the UI thread.
  void finishPendingFetchInvite() {
    if (waitingToFetchInvites) {
      waitingToFetchInvites = false;

      synchronized (googleApiLock) {
        AppInvite.AppInviteApi.getInvitation(googleApiClient, null, false)
            .setResultCallback(
                new ResultCallback<AppInviteInvitationResult>() {
                  @Override
                  public void onResult(AppInviteInvitationResult result) {
                    Log.d(TAG, "getInvitation:onResult:" + result.getStatus());
                    Intent invitation = result.getInvitationIntent();
                    if (invitation != null && AppInviteReferral.hasReferral(invitation)) {
                      // We got referral data, so there is either
                      // an invitation ID or deep link or both.
                      String deepLink = AppInviteReferral.getDeepLink(invitation);
                      // Bug workaround: not setting a deep link causes things to explode.
                      // TODO(jsimantov): Remove this when App Invites in GMSCore is fixed,
                      // b/27612427
                      if (deepLink != null && deepLink.equals(NULL_DEEP_LINK_URL)) {
                        deepLink = null;
                      }
                      safeReceivedInviteCallback(
                          nativeInternalPtr,
                          AppInviteReferral.getInvitationId(invitation),
                          deepLink,
                          0,
                          null);
                    } else if (result.getStatus().isSuccess() || result.getStatus().isCanceled()) {
                      // These mean there is simply no invite.
                      safeReceivedInviteCallback(nativeInternalPtr, null, null, 0, null);
                    } else {
                      // Otherwise, something odd happened, so pass through the
                      // status code.
                      String statusMessage = result.getStatus().getStatusMessage();
                      if (statusMessage == null) {
                        statusMessage = result.getStatus().toString();
                      }
                      safeReceivedInviteCallback(
                          nativeInternalPtr,
                          null,
                          null,
                          result.getStatus().getStatusCode(),
                          statusMessage);
                    }
                  }
                });
      }
    }
  }

  // Fetch invites - this function is called from JNI to trigger checking for invites.
  public boolean fetchInvite() {
    waitingToFetchInvites = true;
    try {
      activity.runOnUiThread(
          new Runnable() {
            @Override
            public void run() {
              boolean doFinish = false;
              synchronized (googleApiLock) {
                if (googleApiClient != null && googleApiClient.isConnected()) {
                  doFinish = true;
                }
              }
              if (doFinish) {
                finishPendingFetchInvite();
              }
              // otherwise the onConnected callback will run finishPendingFetchInvite.
            }
          });
      return true;
    } catch (Exception e) {
      // Failed to start fetching invites. The client code will handle this when we return false..
      return false;
    }
  }

  // Mark a previously-received invite as "converted".
  // This function is called from JNI to trigger invite conversion.
  public boolean convertInvitation(String invitationId) {
    try {
      final String invitationIdFinal = invitationId;
      activity.runOnUiThread(
          new Runnable() {
            @Override
            public void run() {
              synchronized (googleApiLock) {
                AppInvite.AppInviteApi.convertInvitation(googleApiClient, invitationIdFinal)
                    .setResultCallback(
                        new ResultCallback<Status>() {
                          @Override
                          public void onResult(Status status) {
                            Log.d(TAG, "convertInvitation:onResult:" + status);
                            if (status.isSuccess()) {
                              safeConvertedInviteCallback(
                                  nativeInternalPtr, invitationIdFinal, 0, null);
                            } else {
                              safeConvertedInviteCallback(
                                  nativeInternalPtr,
                                  invitationIdFinal,
                                  status.getStatusCode(),
                                  status.getStatusMessage());
                            }
                          }
                        });
              }
            }
          });
      return true;
    } catch (Exception e) {
      // Error marking the invite as converted.
      // The client code can handle this (via the returned false) as it sees fit.
      return false;
    }
  }

  // Safe wrappers for native callbacks that will log an error if native callbacks are missing.
  public static void safeConnectionFailedCallback(
      long nativeInternalPtr, int errorCode, String errorMessage) {
    try {
      connectionFailedCallback(nativeInternalPtr, errorCode, errorMessage);
    } catch (UnsatisfiedLinkError e) {
      String missingDetails = String.format(
          "GoogleApiClient connection failed (code=%d, message=%s)", errorCode,
          String.valueOf(errorMessage));
      Log.e(TAG, String.format(NATIVE_METHOD_MISSING_MESSAGE, missingDetails, e.toString()));
    }
  }

  public static void safeSentInviteCallback(
      long nativeInternalPtr, String[] invitationIDs, int errorCode, String errorString) {
    try {
      sentInviteCallback(nativeInternalPtr, invitationIDs, errorCode, errorString);
    } catch (UnsatisfiedLinkError e) {
      String missingDetails = String.format("invite IDs (code=%d, message=%s", errorCode,
          String.valueOf(errorString));
      Log.e(TAG, String.format(NATIVE_METHOD_MISSING_MESSAGE, missingDetails, e.toString()));
    }
  }

  public static void safeReceivedInviteCallback(
      long nativeInternalPtr,
      String invitationId,
      String deepLinkUrl,
      int resultCode,
      String errorString) {
    try {
      receivedInviteCallback(nativeInternalPtr, invitationId, deepLinkUrl, resultCode, errorString);
    } catch (UnsatisfiedLinkError e) {
      String missingDetails = String.format("received invite id=%s url=%s (code=%d, message=%s)",
          String.valueOf(invitationId), String.valueOf(deepLinkUrl), resultCode, errorString);
      Log.e(TAG, String.format(NATIVE_METHOD_MISSING_MESSAGE, missingDetails, e.toString()));
    }
  }

  public static void safeConvertedInviteCallback(
      long nativeInternalPtr, String invitationId, int resultCode, String errorString) {
    try {
      convertedInviteCallback(nativeInternalPtr, invitationId, resultCode, errorString);
    } catch (UnsatisfiedLinkError e) {
      String missingDetails = String.format("converted invite id=%s (errorCode=%d errorMessage=%s)",
          String.valueOf(invitationId), resultCode, errorString);
      Log.e(TAG, String.format(NATIVE_METHOD_MISSING_MESSAGE, missingDetails, e.toString()));
    }
  }

  // Native callbacks, JNI will register native functions to these.
  public static native void connectionFailedCallback(
      long nativeInternalPtr, int errorCode, String errorMessage);

  public static native void sentInviteCallback(
      long nativeInternalPtr, String[] invitationIDs, int errorCode, String errorString);

  public static native void receivedInviteCallback(
      long nativeInternalPtr,
      String invitationId,
      String deepLinkUrl,
      int resultCode,
      String errorString);

  public static native void convertedInviteCallback(
      long nativeInternalPtr, String invitationId, int resultCode, String errorString);
}
