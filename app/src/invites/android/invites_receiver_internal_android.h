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

// Internal header file for Android Firebase invites sending functionality.

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_INVITES_ANDROID_INVITES_RECEIVER_INTERNAL_ANDROID_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_INVITES_ANDROID_INVITES_RECEIVER_INTERNAL_ANDROID_H_

#include <jni.h>

#include "app/src/invites/android/invites_android_helper.h"
#include "app/src/invites/invites_receiver_internal.h"

namespace firebase {
class App;

namespace invites {
namespace internal {

class InvitesReceiverInternalAndroid : public InvitesReceiverInternal {
 public:
  InvitesReceiverInternalAndroid(const ::firebase::App& app);
  virtual ~InvitesReceiverInternalAndroid() {}

  virtual bool PerformFetch();

  virtual bool PerformConvertInvitation(const char* invitation_id);

 private:
  AndroidHelper android;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_INVITES_ANDROID_INVITES_RECEIVER_INTERNAL_ANDROID_H_
