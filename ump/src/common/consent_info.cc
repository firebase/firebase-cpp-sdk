/*
 * Copyright 2023 Google LLC
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

#include "firebase/ump/consent_info.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>

#include "app/src/assert.h"
#include "app/src/no_destructor.h"
#include "firebase/app.h"
#include "firebase/internal/platform.h"
#include "firebase/ump.h"
#include "ump/src/common/consent_info_internal.h"

namespace firebase {
namespace ump {

namespace {

struct ConsentInfoRegistry {
  std::mutex mutex;
  std::condition_variable cv;
  std::weak_ptr<internal::ConsentInfoInternal> instance;
  // True from the moment a creation begins until its custom deleter finishes
  // running `delete ptr`, preventing a concurrent `GetInstance()` from
  // observing a half-destroyed platform singleton or racing with Android
  // `ReleaseClasses`/`util::Terminate`.
  bool active_or_destroying = false;
};

ConsentInfoRegistry& GetRegistry() {
  static ::firebase::internal::NoDestructor<ConsentInfoRegistry> registry;
  return *registry;
}

void DeleteConsentInfoInternal(internal::ConsentInfoInternal* ptr) {
  delete ptr;
  auto& registry = GetRegistry();
  {
    std::lock_guard<std::mutex> lock(registry.mutex);
    registry.active_or_destroying = false;
  }
  registry.cv.notify_all();
}

template <typename CreateFn>
std::shared_ptr<internal::ConsentInfoInternal> GetOrCreateConsentInfo(
    CreateFn&& create_fn, ::firebase::InitResult* init_result_out) {
  auto& registry = GetRegistry();
  std::unique_lock<std::mutex> lock(registry.mutex);
  for (;;) {
    if (auto existing = registry.instance.lock()) {
      if (init_result_out) *init_result_out = kInitResultSuccess;
      return existing;
    }
    if (!registry.active_or_destroying) {
      break;
    }
    registry.cv.wait(lock);
  }

  registry.active_or_destroying = true;
  lock.unlock();

  internal::ConsentInfoInternal* raw = create_fn();
  if (!raw) {
    {
      std::lock_guard<std::mutex> relock(registry.mutex);
      registry.active_or_destroying = false;
    }
    registry.cv.notify_all();
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return {};
  }

  std::shared_ptr<internal::ConsentInfoInternal> created(
      raw, &DeleteConsentInfoInternal);
  if (!created->PostConstruct()) {
    // Dropping `created` invokes `DeleteConsentInfoInternal`, which cleans up
    // the instance, clears `active_or_destroying`, and wakes any waiters.
    created.reset();
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return {};
  }

  {
    std::lock_guard<std::mutex> relock(registry.mutex);
    registry.instance = created;
  }
  registry.cv.notify_all();
  if (init_result_out) *init_result_out = kInitResultSuccess;
  return created;
}

}  // namespace

ConsentInfo ConsentInfo::GetInstance(const ::firebase::App& app,
                                     ::firebase::InitResult* init_result_out) {
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    if (auto existing = registry.instance.lock()) {
      if (init_result_out) *init_result_out = kInitResultSuccess;
      return ConsentInfo(std::move(existing));
    }
  }
#if FIREBASE_PLATFORM_ANDROID
  if (!app.is_valid()) {
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return {};
  }
  return GetInstance(app.GetJNIEnv(), app.activity(), init_result_out);
#else   // !FIREBASE_PLATFORM_ANDROID
  (void)app;
  return GetInstance(init_result_out);
#endif  // FIREBASE_PLATFORM_ANDROID
}

#if FIREBASE_PLATFORM_ANDROID
ConsentInfo ConsentInfo::GetInstance() {
  auto& registry = GetRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  return ConsentInfo(registry.instance.lock());
}

ConsentInfo ConsentInfo::GetInstance(JNIEnv* jni_env, jobject activity,
                                     ::firebase::InitResult* init_result_out) {
  return ConsentInfo(GetOrCreateConsentInfo(
      [jni_env, activity]() {
        return internal::ConsentInfoInternal::CreateInstance(jni_env, activity);
      },
      init_result_out));
}
#else   // !FIREBASE_PLATFORM_ANDROID
ConsentInfo ConsentInfo::GetInstance(::firebase::InitResult* init_result_out) {
  return ConsentInfo(GetOrCreateConsentInfo(
      []() { return internal::ConsentInfoInternal::CreateInstance(); },
      init_result_out));
}
#endif  // FIREBASE_PLATFORM_ANDROID

ConsentInfo::ConsentInfo(
    std::shared_ptr<internal::ConsentInfoInternal> internal)
    : internal_(std::move(internal)) {}

ConsentInfo::~ConsentInfo() = default;

// Below this, everything is a passthrough to ConsentInfoInternal. If there is
// no internal_ pointer (e.g. it's been cleaned up), return default values and
// invalid futures.

ConsentStatus ConsentInfo::GetConsentStatus() {
  if (!internal_) return kConsentStatusUnknown;
  return internal_->GetConsentStatus();
}

ConsentFormStatus ConsentInfo::GetConsentFormStatus() {
  if (!internal_) return kConsentFormStatusUnknown;
  return internal_->GetConsentFormStatus();
}

Future<void> ConsentInfo::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  if (!internal_) return Future<void>();
  return internal_->RequestConsentInfoUpdate(params);
}

Future<void> ConsentInfo::RequestConsentInfoUpdateLastResult() {
  if (!internal_) return Future<void>();
  return internal_->RequestConsentInfoUpdateLastResult();
}

Future<void> ConsentInfo::LoadConsentForm() {
  if (!internal_) return Future<void>();
  return internal_->LoadConsentForm();
}

Future<void> ConsentInfo::LoadConsentFormLastResult() {
  if (!internal_) return Future<void>();
  return internal_->LoadConsentFormLastResult();
}

Future<void> ConsentInfo::ShowConsentForm(FormParent parent) {
  if (!internal_) return Future<void>();
  return internal_->ShowConsentForm(parent);
}

Future<void> ConsentInfo::ShowConsentFormLastResult() {
  if (!internal_) return Future<void>();
  return internal_->ShowConsentFormLastResult();
}

Future<void> ConsentInfo::LoadAndShowConsentFormIfRequired(FormParent parent) {
  if (!internal_) return Future<void>();
  return internal_->LoadAndShowConsentFormIfRequired(parent);
}

Future<void> ConsentInfo::LoadAndShowConsentFormIfRequiredLastResult() {
  if (!internal_) return Future<void>();
  return internal_->LoadAndShowConsentFormIfRequiredLastResult();
}

PrivacyOptionsRequirementStatus
ConsentInfo::GetPrivacyOptionsRequirementStatus() {
  if (!internal_) return kPrivacyOptionsRequirementStatusUnknown;
  return internal_->GetPrivacyOptionsRequirementStatus();
}

Future<void> ConsentInfo::ShowPrivacyOptionsForm(FormParent parent) {
  if (!internal_) return Future<void>();
  return internal_->ShowPrivacyOptionsForm(parent);
}

Future<void> ConsentInfo::ShowPrivacyOptionsFormLastResult() {
  if (!internal_) return Future<void>();
  return internal_->ShowPrivacyOptionsFormLastResult();
}

bool ConsentInfo::CanRequestAds() {
  if (!internal_) return false;
  return internal_->CanRequestAds();
}

void ConsentInfo::Reset() {
  if (!internal_) return;
  internal_->Reset();
}

}  // namespace ump
}  // namespace firebase
