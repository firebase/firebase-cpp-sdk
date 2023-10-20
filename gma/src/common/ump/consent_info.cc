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

#include "firebase/gma/ump/consent_info.h"

#include "app/src/assert.h"
#include "firebase/app.h"
#include "firebase/gma/ump.h"
#include "firebase/internal/platform.h"
#include "gma/src/common/ump/consent_info_internal.h"

namespace firebase {
namespace gma {
namespace ump {

ConsentInfo* ConsentInfo::s_instance_ = nullptr;

ConsentInfo* ConsentInfo::GetInstance(const ::firebase::App& app,
                                      ::firebase::InitResult* init_result_out) {
  if (s_instance_) {
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return s_instance_;
  }
#if FIREBASE_PLATFORM_ANDROID
  return GetInstance(app.GetJNIEnv(), app.activity(), init_result_out);
#else   // !FIREBASE_PLATFORM_ANDROID
  return GetInstance(init_result_out);
#endif  // FIREBASE_PLATFORM_ANDROID
}

#if FIREBASE_PLATFORM_ANDROID
ConsentInfo* ConsentInfo::GetInstance() { return s_instance_; }

ConsentInfo* ConsentInfo::GetInstance(JNIEnv* jni_env, jobject activity,
                                      ::firebase::InitResult* init_result_out) {
#else  // !FIREBASE_PLATFORM_ANDROID
ConsentInfo* ConsentInfo::GetInstance(::firebase::InitResult* init_result_out) {
#endif
  if (s_instance_) {
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return s_instance_;
  }

  ConsentInfo* consent_info = new ConsentInfo();
#if FIREBASE_PLATFORM_ANDROID
  InitResult result = consent_info->Initialize(jni_env, activity);
#else
  InitResult result = consent_info->Initialize();
#endif
  if (init_result_out) *init_result_out = result;
  if (result != kInitResultSuccess) {
    delete consent_info;
    return nullptr;
  }
  return consent_info;
}

ConsentInfo::ConsentInfo() {
  internal_ = nullptr;
#if FIREBASE_PLATFORM_ANDROID
  java_vm_ = nullptr;
#endif
  s_instance_ = this;
}

ConsentInfo::~ConsentInfo() {
  if (internal_) {
    delete internal_;
    internal_ = nullptr;
  }
  s_instance_ = nullptr;
}

#if FIREBASE_PLATFORM_ANDROID
InitResult ConsentInfo::Initialize(JNIEnv* jni_env, jobject activity) {
  FIREBASE_ASSERT(!internal_);
  internal_ = internal::ConsentInfoInternal::CreateInstance(jni_env, activity);
  return internal_ ? kInitResultSuccess : kInitResultFailedMissingDependency;
}
#else
InitResult ConsentInfo::Initialize() {
  FIREBASE_ASSERT(!internal_);
  internal_ = internal::ConsentInfoInternal::CreateInstance();
  return kInitResultSuccess;
}
#endif

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
}  // namespace gma
}  // namespace firebase
