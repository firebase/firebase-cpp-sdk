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

// Internal header file for shared Android functionality.

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_INVITES_ANDROID_INVITES_ANDROID_HELPER_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_INVITES_ANDROID_INVITES_ANDROID_HELPER_H_

#include <jni.h>
#include "app/src/include/firebase/app.h"
#include "app/src/invites/sender_receiver_interface.h"
#include "app/src/mutex.h"
#include "app/src/util_android.h"

namespace firebase {
namespace invites {
namespace internal {

// clang-format off
#define INVITE_METHODS(X) \
  X(Constructor,                                                \
    "<init>",                                                   \
    "(JLandroid/app/Activity;"                                  \
      "Lcom/google/android/gms/common/api/GoogleApiClient;)V"), \
  X(DiscardNativePointer, "discardNativePointer", "()V"),       \
  X(ShowSenderUI, "showSenderUI", "()Z"),                       \
  X(SetInvitationOption,                                        \
    "setInvitationOption",                                      \
    "(Ljava/lang/String;Ljava/lang/String;)V"),                 \
  X(GetInvitationOptionMaxLength,                               \
    "getInvitationOptionMaxLength",                             \
    "(Ljava/lang/String;)I"),                                   \
  X(GetInvitationOptionMinLength,                               \
    "getInvitationOptionMinLength",                             \
    "(Ljava/lang/String;)I"),                                   \
  X(ClearInvitationOptions, "clearInvitationOptions", "()V"),   \
  X(AddReferralParam,                                           \
    "addReferralParam",                                         \
    "(Ljava/lang/String;Ljava/lang/String;)V"),                 \
  X(ClearReferralParams, "clearReferralParams", "()V"),         \
  X(ResetSenderSettings, "resetSenderSettings", "()V"),         \
  X(FetchInvite, "fetchInvite", "()Z"),                         \
  X(ConvertInvitation,                                          \
    "convertInvitation",                                        \
    "(Ljava/lang/String;)Z")
// clang-format on
METHOD_LOOKUP_DECLARATION(invite, INVITE_METHODS)

class AndroidHelper {
 public:
  AndroidHelper(const ::firebase::App& app,
                SenderReceiverInterface* sender_receiver_interface);
  ~AndroidHelper();

  // The AppInviteNativeWrapper Java object we have instantiated.
  jobject wrapper_obj() { return wrapper_obj_; }

  // JNI helper functions

  // Call a method returning boolean with no parameters.
  bool CallBooleanMethod(invite::Method method);

  // Call a method returning boolean with a string parameter.
  bool CallBooleanMethodString(invite::Method method, const char* strparam);

  // Call a method returning integer with a string parameter.
  int CallIntMethodString(invite::Method method, const char* strparam);

  // Call a method returning void, with no parameters.
  void CallMethod(invite::Method method);

  // Call a method returning void, with two string parameters.
  void CallMethodStringString(invite::Method method, const char* strparam1,
                              const char* strparam2);

  void CheckJNIException();

  void ConnectionFailedCallback(int error_code);

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

 private:
  // The App contains a global reference to the Activity, as well as access to
  // the JNIEnv.
  const ::firebase::App* app_;

  // The instantiated AppInviteNativeWrapper object. This is a global reference,
  // so we must destroy it when we are finished.
  jobject wrapper_obj_;

  // Instantiate wrapper_obj_ by calling the wrapper class constructor.
  void CreateWrapperObject(SenderReceiverInterface* sender_receiver_interface);

  static Mutex init_mutex_;  // NOLINT
  static int initialize_count_;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_INVITES_ANDROID_INVITES_ANDROID_HELPER_H_
