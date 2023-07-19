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
#include "firebase/internal/platform.h"
#include "firebase/gma/ump.h"
#include "gma/src/common/ump/consent_info_internal.h"

namespace firebase {
namespace gma {
namespace ump {

ConsentInfo* ConsentInfo::s_instance_ = nullptr;

ConsentInfo* ConsentInfo::GetInstance(const ::firebase::App& app,
				      ::firebase::InitResult *init_result_out) {
  if (s_instance_) {
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return s_instance_;
  }
#if FIREBASE_PLATFORM_ANDROID
  return GetInstance(app.GetJNIEnv(), app.activity(), init_result_out);
#else  // !FIREBASE_PLATFORM_ANDROID
  return GetInstance(init_result_out);
#endif  // FIREBASE_PLATFORM_ANDROID
};
  
#if FIREBASE_PLATFORM_ANDROID
ConsentInfo* ConsentInfo::GetInstance(JNIEnv* jni_env, jobject activity,
				      ::firebase::InitResult *init_result_out) {
#else  // !FIREBASE_PLATFORM_ANDROID
ConsentInfo* ConsentInfo::GetInstance(::firebase::InitResult *init_result_out) {
#endif
  if (s_instance_) {
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return s_instance_;
  }

  ConsentInfo* consent_info = new ConsentInfo();
#if FIREBASE_PLATFORM_ANDROID
  InitResult result = consent_info->Initialize(/* jni_env, activity */);  // TODO
#else
  InitResult result = consent_info->Initialize();
#endif
  if (result != kInitResultSuccess) {
    if (init_result_out) *init_result_out = result;
    delete consent_info;
    return nullptr;
  }
  return consent_info;
};
 
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

InitResult ConsentInfo::Initialize() {
  FIREBASE_ASSERT(internal_ == nullptr);
  internal_ = internal::ConsentInfoInternal::CreateInstance();
  return kInitResultSuccess;
}

ConsentStatus ConsentInfo::GetConsentStatus() {
  FIREBASE_ASSERT(internal_);
  return internal_->GetConsentStatus();
}

ConsentFormStatus ConsentInfo::GetConsentFormStatus() {
  FIREBASE_ASSERT(internal_);
  return internal_->GetConsentFormStatus();
}
  
Future<ConsentStatus> ConsentInfo::RequestConsentStatus(const ConsentRequestParameters& params) {
  FIREBASE_ASSERT(internal_);
  return internal_->RequestConsentStatus(params);
}

Future<ConsentFormStatus> ConsentInfo::LoadConsentForm() {
  FIREBASE_ASSERT(internal_);
  return internal_->LoadConsentForm();
}

Future<ConsentStatus> ConsentInfo::ShowConsentForm(FormParent parent) {
  return internal_->ShowConsentForm(parent);
}

void ConsentInfo::Reset() {
  return internal_->Reset();
}


 
}  // namespace ump
}  // namespace gma
}  // namespace firebase


