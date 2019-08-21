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

#include "app/src/invites/invites_receiver_internal.h"

#include <assert.h>

#include <algorithm>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/reference_counted_future_impl.h"
#if FIREBASE_PLATFORM_ANDROID
#include "app/src/invites/android/invites_receiver_internal_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "app/src/invites/ios/invites_receiver_internal_ios.h"
#else
#include "app/src/invites/stub/invites_receiver_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

namespace firebase {
namespace invites {
namespace internal {

static const int kFetchFailedCode = -1;
static const char kFetchFailedMessage[] = "Dynamic link fetch failed.";
static const int kConvertFailedCode = -1;
static const char kConvertFailedMessage[] = "Invite conversion failed.";
static const int kConvertInProgressCode = -2;
static const char kConvertInProgressMessage[] =
    "Invite conversion already in progress";

// Used by testing and reference counting the receiver singleton.
// When testing this is used by SetNextCreatedInstance() to set the next
// instance returned by CreateInstance().
static InvitesReceiverInternal* g_receiver = nullptr;

InvitesReceiverInternal* InvitesReceiverInternal::CreateInstance(
    const ::firebase::App& app, ReceiverInterface* receiver_implementation) {
  InvitesReceiverInternal* receiver = g_receiver;
  if (!receiver) {
#if FIREBASE_PLATFORM_ANDROID
    receiver = new InvitesReceiverInternalAndroid(app);
#elif FIREBASE_PLATFORM_IOS
    receiver = new InvitesReceiverInternalIos(app);
#else
    receiver = new InvitesReceiverInternalStub(app);
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS
    if (!receiver->initialized()) {
      delete receiver;
      return nullptr;
    }
    g_receiver = receiver;
  }
  receiver->receiver_implementations_.push_back(receiver_implementation);
  receiver->ref_count_++;
  // Notify the newly registered receiver of any cached notifications.
  receiver->cached_receiver_.NotifyReceiver(receiver_implementation);
  return receiver;
}

void InvitesReceiverInternal::DestroyInstance(
    InvitesReceiverInternal* receiver,
    ReceiverInterface* receiver_implementation) {
  assert(receiver && receiver == g_receiver);
  assert(receiver->initialized());
  assert(receiver->ref_count_);
  if (receiver_implementation) {
    auto& receiver_implementations = receiver->receiver_implementations_;
    auto it =
        std::find(receiver_implementations.begin(),
                  receiver_implementations.end(), receiver_implementation);
    if (it != receiver_implementations.end()) {
      receiver_implementations.erase(it);
    }
  }
  receiver->ref_count_--;
  if (receiver->ref_count_ == 0) {
    delete receiver;
    g_receiver = nullptr;
  }
}

void InvitesReceiverInternal::SetNextCreatedInstance(
    InvitesReceiverInternal* instance) {
  g_receiver = instance;
}

void InvitesReceiverInternal::Fetch() {
  if (!PerformFetch()) {
    ReceivedInviteCallback("", "", kLinkMatchStrengthNoMatch, kFetchFailedCode,
                           kFetchFailedMessage);
  }
}

void InvitesReceiverInternal::ReceivedInviteCallback(
    const std::string& invitation_id, const std::string& deep_link_url,
    InternalLinkMatchStrength match_strength, int result_code,
    const std::string& error_message) {
  LogDebug(
      "Received link: invite_id=%s url=%s match_strength=%d result=%d "
      "error=%s",
      invitation_id.c_str(), deep_link_url.c_str(),
      static_cast<int>(match_strength), result_code, error_message.c_str());
  for (auto it = receiver_implementations_.begin();
       it != receiver_implementations_.end(); ++it) {
    (*it)->ReceivedInviteCallback(invitation_id, deep_link_url, match_strength,
                                  result_code, error_message);
  }
}

Future<void> InvitesReceiverInternal::ConvertInvitation(
    const char* invitation_id) {
  if (!future_impl_.ValidFuture(future_handle_convert_)) {
    future_handle_convert_ = future_impl_.SafeAlloc<void>(kInvitesFnConvert);
    if (!PerformConvertInvitation(invitation_id)) {
      future_impl_.Complete(future_handle_convert_, kConvertFailedCode,
                            kConvertFailedMessage);
      // This will tell all of the pending Futures that we have failed. Once all
      // those futures are gone, the RefFuture will automatically be deleted.
      future_handle_convert_ = SafeFutureHandle<void>::kInvalidHandle;
    }
  } else {
    // If there's already a convert in progress, fail.
    const SafeFutureHandle<void> handle =
        future_impl_.SafeAlloc<void>(kInvitesFnConvert);
    future_impl_.Complete(handle, kConvertInProgressCode,
                          kConvertInProgressMessage);
  }
  return ConvertInvitationLastResult();
}

Future<void> InvitesReceiverInternal::ConvertInvitationLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kInvitesFnConvert));
}

void InvitesReceiverInternal::ConvertedInviteCallback(
    const std::string& invitation_id, int result_code,
    std::string error_message) {
  future_impl_.Complete(future_handle_convert_, result_code,
                        error_message.c_str());
  future_handle_convert_ = SafeFutureHandle<void>::kInvalidHandle;
}

}  // namespace internal
}  // namespace invites
}  // namespace firebase
