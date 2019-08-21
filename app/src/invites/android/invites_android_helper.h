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
#define DYNAMIC_LINKS_NATIVE_WRAPPER_METHODS(X) \
  X(Constructor,                                                \
    "<init>",                                                   \
    "(JLandroid/app/Activity;)V"), \
  X(DiscardNativePointer, "discardNativePointer", "()V"),       \
  X(FetchDynamicLink, "fetchDynamicLink", "()Z")
// clang-format on
METHOD_LOOKUP_DECLARATION(dynamic_links_native_wrapper,
                          DYNAMIC_LINKS_NATIVE_WRAPPER_METHODS)

class AndroidHelper {
 public:
  AndroidHelper(const ::firebase::App& app,
                SenderReceiverInterface* sender_receiver_interface);
  ~AndroidHelper();

  // The DynamicLinksNativeWrapper Java object we have instantiated.
  jobject wrapper_obj() { return wrapper_obj_; }

  // JNI helper functions

  // Call a method returning boolean with no parameters.
  bool CallBooleanMethod(dynamic_links_native_wrapper::Method method);

  // Call a method returning boolean with a string parameter.
  bool CallBooleanMethodString(dynamic_links_native_wrapper::Method method,
                               const char* strparam);

  // Call a method returning integer with a string parameter.
  int CallIntMethodString(dynamic_links_native_wrapper::Method method,
                          const char* strparam);

  // Call a method returning void, with no parameters.
  void CallMethod(dynamic_links_native_wrapper::Method method);

  // Call a method returning void, with two string parameters.
  void CallMethodStringString(dynamic_links_native_wrapper::Method method,
                              const char* strparam1, const char* strparam2);

  void CheckJNIException();

  void ConnectionFailedCallback(int error_code);

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

 private:
  // The App contains a global reference to the Activity, as well as access to
  // the JNIEnv.
  const ::firebase::App* app_;

  // The instantiated DynamicLinksNativeWrapper object. This is a global
  // reference, so we must destroy it when we are finished.
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
