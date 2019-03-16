#ifndef FIREBASE_INVITES_CLIENT_CPP_INCLUDE_FIREBASE_INVITES_H_
#define FIREBASE_INVITES_CLIENT_CPP_INCLUDE_FIREBASE_INVITES_H_

// Copyright 2016 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <map>
#include <string>
#include <vector>
#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

#if !defined(DOXYGEN) && !defined(SWIG)
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(invites)
#endif  // !defined(DOXYGEN) && !defined(SWIG)

namespace firebase {

/// @brief Firebase Invites API.
///
/// Firebase Invites is a cross-platform solution for sending personalized
/// email and SMS invitations, on-boarding users, and measuring the impact
/// of invitations or dynamic links.
///
/// @deprecated Firebase Invites is deprecated. Please refer to
/// https://firebase.google.com/docs/invites for details.
namespace invites {

/// @brief Initialize the Firebase Invites library.
///
/// You must call this in order to send and receive invites.
///
/// @return kInitResultSuccess if initialization succeeded, or
/// kInitResultFailedMissingDependency on Android if Google Play services is
/// not available on the current device.
///
/// @deprecated Firebase Invites is deprecated. Please refer to
/// https://firebase.google.com/docs/invites for details.
FIREBASE_DEPRECATED InitResult Initialize(const App& app);

/// @brief Terminate the Invites API.
///
/// Cleans up resources associated with the API.
void Terminate();

class InvitesSharedData;

/// @brief Enum describing the strength of a dynamic links match.
///
/// This version is local to invites; it mirrors the enum
/// firebase::dynamic_links::LinkMatchStrength in dynamic_links.
enum LinkMatchStrength {
  /// No match has been achieved
  kLinkMatchStrengthNoMatch = 0,

  /// The match between the Dynamic Link and device is not perfect.  You should
  /// not reveal any personal information related to the Dynamic Link.
  kLinkMatchStrengthWeakMatch,

  /// The match between the Dynamic Link and this device has a high confidence,
  /// but there is a small possibility of error.
  kLinkMatchStrengthStrongMatch,

  /// The match between the Dynamic Link and the device is exact.  You may
  /// safely reveal any personal information related to this Dynamic Link.
  kLinkMatchStrengthPerfectMatch
};

namespace internal {
// Forward declaration for platform-specific data, implemented in each library.
class InvitesSenderInternal;
}  // namespace internal

/// @brief Data structure used to construct and send an invite.
///
/// @if cpp_examples
/// @see firebase::invites::SendInvite()
/// @endif
/// <SWIG>
/// @if swig_examples
/// @see FirebaseInvites.SendInviteAsync()
/// @endif
/// </SWIG>
struct Invite {
  // LINT.IfChange
  /// @brief Maximum length for an invitation message.
  ///
  /// @if cpp_examples
  /// @see Invite::message_text.
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// @see Invite.MessageText.
  /// @endif
  /// </SWIG>
  static const unsigned int kMaxMessageLength = 100;

  /// @brief Maximum length for an HTML invitation message.
  ///
  /// @if cpp_examples
  /// @see email_content_html
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// @see Invite.EmailContentHtml
  /// @endif
  /// </SWIG>
  static const unsigned int kMaxEmailHtmlContentLength = 512000;

  /// @brief Minimum length for the call to action button.
  ///
  /// @if cpp_examples
  /// @see Invite::call_to_action_text
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// @see Invite.CallToActionText
  /// @endif
  /// </SWIG>
  static const unsigned int kMinCallToActionTextLength = 2;

  /// @brief Maximum length for the call to action button.
  ///
  /// @if cpp_examples
  /// @see Invite::call_to_action_text
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// @see Invite.CallToActionText
  /// @endif
  /// </SWIG>
  static const unsigned int kMaxCallToActionTextLength = 20;

  /// @brief Maximum length for the app description.
  ///
  /// @if cpp_examples
  /// @see Invite::description_text
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// @see Invite.DescriptionText
  /// @endif
  /// </SWIG>
  static const unsigned int kMaxDescriptionTextLength = 1000;
  // LINT.ThenChange(//depot_firebase_cpp/invites/client/cpp/src/include/firebase/csharp/invites.SWIG)

  /// Initialize the invite.
  FIREBASE_DEPRECATED Invite() : android_minimum_version_code(0) {}

  /// @brief Optional minimum version of the android app installed on the
  /// receiving device.
  ///
  /// If you don't specify this, any Android version will be allowed.
  int android_minimum_version_code;

  /// @brief Text shown on the email invitation button for the user to
  /// accept the invitation.
  ///
  /// Default text will be used if this is not set.
  ///
  /// @note The length of this text must not exceed kMaxCallToActionTextLength
  /// characters, and must be no shorter than kMinCallToActionTextLength
  /// characters.
  std::string call_to_action_text;

  /// @brief The URL for an image to include in the invitation.
  std::string custom_image_url;

  /// @brief An optional dynamic link that will be sent with the invitation.
  ///
  /// If you don't specify this, your invite will have no dynamic link.
  std::string deep_link_url;

  /// @brief The app description text for email invitations.
  ///
  /// @note The length of this text must not exceed kMaxDescriptionTextLength
  /// characters.
  ///
  /// @note This function is for iOS only. On Android, this setting will be
  /// ignored, and your app's description will be automatically populated from
  /// its Google Play listing.
  std::string description_text;

  /// @brief The full HTML content of the invitation that will be sent.
  ///
  /// This should be properly-formatted UTF8 HTML with no JavaScript. The
  /// pattern %%APPINVITE_LINK_PLACEHOLDER%% will be replaced with the
  /// invitation URL.
  ///
  /// This takes precendence over the text functions message_text,
  /// call_to_action_text, and custom_image_url. If you want full control
  /// over the contents of the invitation, you should use this.
  ///
  /// If you use this, you must also set email_subject_text or the
  /// HTML content will be ignored.
  ///
  /// If you do use these HTML text fields, they will take priority over the
  /// standard text fields, if HTML is supported on your platform. You probably
  /// still want to set message_text and the other text fields in case your
  /// platform doesn't support HTML.
  ///
  /// @note HTML invitation content is only supported on Android.
  ///
  /// @note The length of the HTML email content must not exceed
  /// kMaxEmailContentHtmlLength characters.
  std::string email_content_html;

  /// @brief The subject text for an HTML e-mail.
  ///
  /// If you use this, you must set email_content_html as well or the
  /// HTML content will be ignored.
  std::string email_subject_text;

  /// @brief The optional Google Analytics Tracking id.
  ///
  /// The tracking id should be created for the calling application under
  /// Google Analytics. The tracking id is recommended so that invitations sent
  /// from the calling application are available in Google Analytics.
  ///
  /// @note This field is only supported on Android. On iOS, if you want
  /// to track invitations in Google Analytics, you will have to do so manually.
  std::string google_analytics_tracking_id;

  /// @brief The text of the invitation message.
  ///
  /// message_text, custom_image_url, and call_to_action_text comprise
  /// the standard text invitation options.
  ///
  /// If you use the standard text methods as well as email_content_html and
  /// email_subject_text, the HTML methods will take priority if your
  /// platform supports them.
  ///
  /// The user is able to modify this message before sending the invite.
  ///
  /// @note The length of this message must not exceed kMaxMessageLength
  /// characters, so it can fit in an SMS message along with the link.
  ///
  /// @note You must set this and title_text or you will not be able to
  /// send an invitation.
  std::string message_text;

  /// @brief The client ID for your app for the Android platform (don't set
  /// this for your current platform).
  ///
  /// Make sure the client ID you specify here matches the client ID for your
  /// project in Google Developer Console for the other platform.
  ///
  /// @note Firebase is smart enough to infer this automatically if your
  /// project in Google Developer Console has only one app for each platform.
  std::string android_platform_client_id;

  /// @brief The client ID for your app for the iOS platform (don't set
  /// this for your current platform).
  ///
  /// Make sure the client ID you specify here matches the client ID for your
  /// project in Google Developer Console for the other platform.
  ///
  /// @note Firebase is smart enough to infer this automatically if your
  /// project in Google Developer Console has only one app for each platform.
  std::string ios_platform_client_id;

  /// @brief Optional additional referral parameters, which is passed to
  /// the invite URL as a key/value pair.
  ///
  /// You can have any number of these.
  ///
  /// These key/value pairs will be included in the referral URL as query
  /// parameters, so they can be read by the app on the receiving side.
  ///
  /// @note Referral parameters are only supported on Android.
  std::map<std::string, std::string> referral_parameters;

  /// @brief The title text for the Invites UI window.
  ///
  /// @note You must set this and message_text to send invitations.
  std::string title_text;
};

/// @brief Results from calling SendInvite() to send an invitation.
///
/// @if cpp_examples
/// This will be returned by SendInvite() inside a Future<SendInviteResult>.
/// In the returned Future, you should check the value of Error() - if it's
/// non-zero, an error occurred while sending invites. You can use the
/// Future's Error() and ErrorMessage() to get more information about what
/// went wrong.
/// @endif
/// <SWIG>
/// @if swig_examples
/// This will be returned by SendInviteAsync() inside a Task<SendInviteResult>.
/// In the returned Task, you should check the value of IsFaulted - if it's
/// true, an error occurred while sending invites. You can use the
/// Task's Exception property to get more information about what went wrong.
/// @endif
/// </SWIG>
struct SendInviteResult {
  /// @brief The Invitation IDs we sent invites to, if any.
  ///
  /// @if cpp_examples
  /// If this is empty, it means the user either chose to back out of the
  /// sending UI without sending invitations (Error() == 0) or something
  /// went wrong (Error() != 0).
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// If this is empty, it means the user either chose to back out of the
  /// sending UI without sending invitations (Task.IsFaulted == false) or
  /// something went wrong (Task.IsFaulted == true).
  /// @endif
  /// </SWIG>
  ///
  /// If this is nonempty, then these invitation IDs will match the invitation
  /// IDs on the receiving side, which may be helpful for analytics purposes.
  std::vector<std::string> invitation_ids;
};

/// @brief Start displaying the invitation UI, which will ultimately result
/// in sending zero or more invitations.
///
/// This will take the invitation settings from the given Invite object,
/// and display a UI to the user where they can share a link
/// to the app with their friends.
///
/// At a minimum, you will need to have set title_text and
/// message_text or the invitation will not be sent.
///
/// @if cpp_examples
/// Usage:
/// @code{.cpp}
/// ::firebase::invites::Invite invite;
/// // ... set fields on invite ...
/// auto send_result = ::firebase::invites::SendInvite(invite);
/// // ... later on ...
/// if (send_result.Status() == kFutureStatusComplete) {
///   if (send_result.Error() == 0) {
///     if (send_result.Result()->invitation_ids.length() > 0) {
///       // Invitations were sent.
///     }
///     else {
///       // User canceled.
///     }
///   }
/// }
/// @endcode
/// @endif
/// <SWIG>
/// @if swig_examples
/// Usage:
/// @code{.cs}
/// Firebase.Invites.Invite invite = new Firebase.Invites.Invite() {
///   TitleText = "Invites Test App",
///   MessageText = "Please try my app! It's awesome.",
///   CallToActionText = "Download it for FREE",
///   DeepLinkUrl = new System.Uri("http://google.com/abc"),
/// };
/// Firebase.Invites.FirebaseInvites.SendInviteAsync(invite).ContinueWith(
///   (sendTask) => {
///     if (sendTask.IsCanceled) {
///       DebugLog("Invitation canceled.");
///     } else if (sendTask.IsFaulted) {
///       DebugLog("Invitation encountered an error:");
///       DebugLog(sendTask.Exception.ToString());
///     } else if (sendTask.IsCompleted) {
///       DebugLog("SendInvite: " +
///       (new List<string>(sendTask.Result.InvitationIDs)).Count +
///       " invites sent successfully.");
///       foreach (string id in sendTask.Result.InvitationIDs) {
///         DebugLog("SendInvite: Invite code: " + id);
///     }
///   });
/// @endcode
/// @endif
/// </SWIG>
///
/// @param[in] invite The Invite that contains the settings to send with.
/// @return A future result telling us whether the invitation was sent.
///
/// @deprecated Firebase Invites is deprecated. Please refer to
/// https://firebase.google.com/docs/invites for details.
FIREBASE_DEPRECATED Future<SendInviteResult> SendInvite(const Invite& invite);

/// @brief Get the results of the previous call to SendInvite. This will stay
/// available until you call SendInvite again.
///
/// @return The future result from the most recent call to SendInvite().
Future<SendInviteResult> SendInviteLastResult();

namespace internal {
// Forward declaration for platform-specific data, implemented in each library.
class InvitesReceiverInternal;
}  // namespace internal

/// @brief Mark the invitation as "converted" in some app-specific way.
///
/// Once you have acted on the invite in some application-specific way, you
/// can call this function to tell Firebase that a "conversion" has occurred
/// and the invite has been acted on in some significant way.
///
/// You don't need to convert immediately when it received, since a
/// "conversion" can happen far in the future from when the invite was
/// initially received, e.g. if it corresponds to the user setting up an
/// account, making a purchase, etc.
///
/// Just save the invitation ID when you initially receive it, and use it
/// later when performing the conversion.
///
/// @if cpp_examples
/// Usage:
/// @code{.cpp}
/// auto convert_result =
///     ::firebase::invites::ConvertInvitation(my_invitation_id);
/// // ... later on ...
/// if (convert_result.Status() == kFutureStatusComplete) {
///   if (convert_result.Error() == 0) {
///     // successfully marked the invitation as converted!
///   }
/// }
/// @endcode
/// @endif
/// <SWIG>
/// @if swig_examples
/// Usage:
/// @code{.cs}
/// public void OnInviteReceived(object sender,
///                              Firebase.Invites.InviteReceivedEventArgs e) {
///   Firebase.Invites.FirebaseInvites.ConvertInvitationAsync(
///     e.InvitationID).ContinueWith((convertTask) => {
///       if (convertTask.IsCanceled) {
///          DebugLog("Conversion canceled.");
///       } else if (convertTask.IsFaulted) {
///          DebugLog("Conversion encountered an error:");
///          DebugLog(convertTask.Exception.ToString());
///       } else if (convertTask.IsCompleted) {
///         DebugLog("Conversion completed successfully!");
///       }
///     });
/// }
/// @endcode
/// @endif
/// </SWIG>
///
/// @param[in] invitation_id The invitation ID to mark as a conversion.
///
/// @return A future result telling you whether the conversion succeeded.
///
/// @deprecated Firebase Invites is deprecated. Please refer to
/// https://firebase.google.com/docs/invites for details.
FIREBASE_DEPRECATED Future<void> ConvertInvitation(const char* invitation_id);

/// Get the (possibly still pending) results of the most recent
/// ConvertInvitation call.
///
/// @return The future result from the last call to ConvertInvitation().
Future<void> ConvertInvitationLastResult();

/// Fetch any pending invites. Each pending invite will trigger a call to the
/// appropriate function on the registered Listener class.
///
/// This function is implicitly called on initialization. On iOS this is called
/// automatically when the app gains focus, but on Android this needs to be
/// called manually.
///
/// @deprecated Firebase Invites is deprecated. Please refer to
/// https://firebase.google.com/docs/invites for details.
FIREBASE_DEPRECATED void Fetch();

/// @brief Base class used to receive Invites and Dynamic Links.
///
/// @deprecated Firebase Invites is deprecated. Please refer to
/// https://firebase.google.com/docs/invites for details.
class Listener {
 public:
  virtual ~Listener() {}

  /// Called when an invitation is received.
  ///
  /// This invitation ID will match the invitation ID on the sender side, so
  /// you can use analytics to determine that the invitation was accepted, and
  /// even save it for future analytical use.
  ///
  /// If Firebase indicates a weak match for a dynamic link, it means that the
  /// match between the dynamic link and the receiving device may not be
  /// perfect. In this case your app should reveal no personal information from
  /// the dynamic link.
  ///
  /// @param[in] invitation_id The Invitation ID. Will be null if there was
  /// just a dynamic link, without an invitation.
  /// @param[in] dynamic_link The Dynamic Link URL. Will be empty if there was
  /// just an App Invite and not a Dynamic Link.
  /// @param[in] is_strong_match Whether the Dynamic Link is a "strong match" or
  /// a "weak match" as defined by the Invites library.
  virtual void OnInviteReceived(const char* invitation_id,
                                const char* dynamic_link, bool is_strong_match);

  /// Called when an invitation is received.
  ///
  /// This invitation ID will match the invitation ID on the sender side, so
  /// you can use analytics to determine that the invitation was accepted, and
  /// even save it for future analytical use.
  ///
  /// If Firebase indicates a weak match for a dynamic link, it means that the
  /// match between the dynamic link and the receiving device may not be
  /// perfect. In this case your app should reveal no personal information from
  /// the dynamic link.
  ///
  /// @param[in] invitation_id The Invitation ID. Will be null if there was
  /// just a dynamic link, without an invitation.
  /// @param[in] dynamic_link The Dynamic Link URL. Will be empty if there was
  /// just an App Invite and not a Dynamic Link.
  /// @param[in] match_strength An enum describing the strength of the match,
  /// as defined by the Invites library.
  virtual void OnInviteReceived(const char* invitation_id,
                                const char* dynamic_link,
                                LinkMatchStrength match_strength);

  /// Called when there was no invitation or dynamic link tied to
  /// opening the app.
  virtual void OnInviteNotReceived() = 0;

  /// Called when an error occurs trying to fetch the invitation information.
  ///
  /// @param[in] error_code The code associated with the error.
  /// @param[in] error_message A description of the error that occurred.
  virtual void OnErrorReceived(int error_code, const char* error_message) = 0;
};

/// @brief Set the listener to handle receiving invitations.
///
/// @param[in] listener A Listener object.
/// @return The previous Listener.
///
/// @deprecated Firebase Invites is deprecated. Please refer to
/// https://firebase.google.com/docs/invites for details.
FIREBASE_DEPRECATED Listener* SetListener(Listener* listener);

}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_INVITES_CLIENT_CPP_INCLUDE_FIREBASE_INVITES_H_
