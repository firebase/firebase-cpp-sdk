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

#include "app/src/invites/android/invites_receiver_internal_android.h"

#include <assert.h>

#include "app/src/include/firebase/app.h"
#include "app/src/invites/android/invites_android_helper.h"
#include "app/src/invites/invites_receiver_internal.h"

namespace firebase {
namespace invites {
namespace internal {

InvitesReceiverInternalAndroid::InvitesReceiverInternalAndroid(
    const ::firebase::App &app)
    : InvitesReceiverInternal(app), android(app, this) {
  if (!android.initialized()) app_ = nullptr;
}

bool InvitesReceiverInternalAndroid::PerformFetch() {
  return android.CallBooleanMethod(
      dynamic_links_native_wrapper::kFetchDynamicLink);
}

bool InvitesReceiverInternalAndroid::PerformConvertInvitation(
    const char * /*invitation_id*/) {
  LogWarning("ConvertInvitation is not implemented.");
  return false;
}

}  // namespace internal
}  // namespace invites
}  // namespace firebase
