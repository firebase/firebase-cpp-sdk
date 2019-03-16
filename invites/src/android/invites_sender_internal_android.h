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

// Internal header file for Android Firebase invites receiving functionality.

#ifndef FIREBASE_INVITES_CLIENT_CPP_SRC_ANDROID_INVITES_SENDER_INTERNAL_ANDROID_H_
#define FIREBASE_INVITES_CLIENT_CPP_SRC_ANDROID_INVITES_SENDER_INTERNAL_ANDROID_H_

#include <jni.h>
#include "app/src/invites/android/invites_android_helper.h"
#include "invites/src/common/invites_sender_internal.h"

namespace firebase {
class App;
namespace invites {
namespace internal {

class InvitesSenderInternalAndroid : public InvitesSenderInternal {
 public:
  InvitesSenderInternalAndroid(const ::firebase::App& app);
  virtual ~InvitesSenderInternalAndroid() {}

  // Platform-specific action: begin showing the UI.
  // Returns true if successful or false if not.
  virtual bool PerformSendInvite();

 private:
  AndroidHelper android;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_INVITES_CLIENT_CPP_SRC_ANDROID_INVITES_SENDER_INTERNAL_ANDROID_H_
