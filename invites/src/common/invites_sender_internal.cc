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

#include "invites/src/common/invites_sender_internal.h"

#include <assert.h>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "invites/src/include/firebase/invites.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

#if defined(__ANDROID__)
#include "invites/src/android/invites_sender_internal_android.h"
#elif TARGET_OS_IPHONE
#include "invites/src/ios/invites_sender_internal_ios.h"
#else
#include "invites/src/stub/invites_sender_internal_stub.h"
#endif  // __ANDROID__, TARGET_OS_IPHONE

namespace firebase {
namespace invites {

extern bool g_initialized;
extern const int kInitErrorNum;
extern const char kInitErrorMsg[];

namespace internal {

static const int kSendInviteFailedCode = -1;
static const char kSendInviteFailedMessage[] =
    "SendInvite() failed, did you specify all necessary options "
    "(such as title and message)?";

static InvitesSenderInternal* g_next_instance = nullptr;

InvitesSenderInternal* InvitesSenderInternal::CreateInstance(
    const ::firebase::App& app) {
  InvitesSenderInternal* sender;
  if (g_next_instance != nullptr) {
    sender = g_next_instance;
    g_next_instance = nullptr;
    return sender;
  }
#if defined(__ANDROID__)
  sender = new InvitesSenderInternalAndroid(app);
#elif TARGET_OS_IPHONE
  sender = new InvitesSenderInternalIos(app);
#else
  sender = new InvitesSenderInternalStub(app);
#endif  // __ANDROID__, TARGET_OS_IPHONE
  if (!sender->initialized()) {
    delete sender;
    return nullptr;
  }
  return sender;
}

void InvitesSenderInternal::SetNextCreatedInstance(
    InvitesSenderInternal* instance) {
  g_next_instance = instance;
}

Future<SendInviteResult> InvitesSenderInternal::SendInvite() {
  if (!future_impl_.ValidFuture(future_handle_send_)) {
    future_handle_send_ =
        future_impl_.Alloc<SendInviteResult>(kInvitesSenderFnSend);

    if (!g_initialized) {
      // Try to initialize. If it fails, return an error.
      if (::firebase::invites::Initialize(*app_) != kInitResultSuccess) {
        future_impl_.Complete(future_handle_send_,
                              firebase::invites::kInitErrorNum,
                              firebase::invites::kInitErrorMsg);
        future_handle_send_ = ReferenceCountedFutureImpl::kInvalidHandle;
        return SendInviteLastResult();
      }
    }
    if (!PerformSendInvite()) {
      future_impl_.Complete(future_handle_send_, kSendInviteFailedCode,
                            kSendInviteFailedMessage);
      // This will tell all of the pending Futures that we have failed. Once all
      // those futures are gone, the RefFuture will automatically be deleted.
      future_handle_send_ = ReferenceCountedFutureImpl::kInvalidHandle;
    }
  }

  // If there's already a convert in progress, we just return that.
  return SendInviteLastResult();
}

Future<SendInviteResult> InvitesSenderInternal::SendInviteLastResult() {
  return static_cast<const Future<SendInviteResult>&>(
      future_impl_.LastResult(kInvitesSenderFnSend));
}

void InvitesSenderInternal::SentInviteCallback(
    const std::vector<std::string>& invitation_ids, int result_code,
    const std::string& error_message) {
  if (result_code != 0) {
    LogError("SendInviteCallback: Error %d: %s", result_code,
             error_message.c_str());
  }

  future_impl_.Complete<SendInviteResult>(
      future_handle_send_, result_code, error_message.c_str(),
      [invitation_ids](SendInviteResult* data) {
        data->invitation_ids = invitation_ids;
      });
  future_handle_send_ = ReferenceCountedFutureImpl::kInvalidHandle;
}

void InvitesSenderInternal::SetInvitationSetting(
    InvitesSenderInternal::InvitationSetting key, const char* new_value) {
  MutexLock lock(invitation_settings_mutex_);
  size_t idx = static_cast<size_t>(key);
  if (invitation_settings_[idx] != nullptr) {
    std::string* old_string = invitation_settings_[idx];
    invitation_settings_[idx] = nullptr;
    delete old_string;
  }

  // At this point, the invitation setting is definitely null.
  if (new_value != nullptr) {
    std::string* new_string = new std::string(new_value);
    invitation_settings_[idx] = new_string;
  }
}

void InvitesSenderInternal::ClearInvitationSettings() {
  MutexLock lock(invitation_settings_mutex_);
  for (size_t i = 0; i < invitation_settings_.size(); i++) {
    if (invitation_settings_[i] != nullptr) {
      std::string* old_string = invitation_settings_[i];
      invitation_settings_[i] = nullptr;
      delete old_string;
    }
  }
  ClearReferralParams();
}

const char* InvitesSenderInternal::GetInvitationSetting(InvitationSetting key) {
  MutexLock lock(invitation_settings_mutex_);
  size_t idx = static_cast<size_t>(key);
  return invitation_settings_[idx] != nullptr
             ? invitation_settings_[idx]->c_str()
             : nullptr;
}

void InvitesSenderInternal::AddReferralParam(const char* key,
                                             const char* value) {
  MutexLock lock(invitation_settings_mutex_);
  assert(key != nullptr);
  if (value != nullptr) {
    referral_parameters_[key] = value;
  } else {
    // delete old value
    referral_parameters_.erase(key);
  }
}

void InvitesSenderInternal::ClearReferralParams() {
  MutexLock lock(invitation_settings_mutex_);
  referral_parameters_.clear();
}

}  // namespace internal
}  // namespace invites
}  // namespace firebase
