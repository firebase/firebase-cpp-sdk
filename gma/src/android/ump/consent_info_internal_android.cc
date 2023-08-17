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

#include "gma/src/android/ump/consent_info_internal_android.h"

#include <vector>

#include "app/src/thread.h"
#include "app/src/util_android.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

// clang-format off
#define CONSENTINFOHELPER_METHODS(X)                              \
  X(Constructor, "<init>", "(JLandroid/app/Activity;)V"),         \
  X(GetConsentStatus, "getConsentStatus", "()I"),                 \
  X(GetConsentStatus, "requestConsentStatusUpdate",               \
    "(ZILjava/util/ArrayList;)V"),                                \
  X(Disconnect, "disconnect", "()V")
// clang-format on

METHOD_LOOKUP_DECLARATION(consent_info_helper, CONSENTINFOHELPER_METHODS);
METHOD_LOOKUP_DEFINITION(
    consent_info_helper,
    "com/google/firebase/gma/internal/cpp/ConsentInfoHelper",
    CONSENTINFOHELPER_METHODS);

// This explicitly implements the constructor for the outer class,
// ConsentInfoInternal.
ConsentInfoInternal* ConsentInfoInternal::CreateInstance(JNIEnv* jni_env,
                                                         jobject activity) {
  ConsentInfoInternalAndroid ptr =
      new ConsentInfoInternalAndroid(jni_env, activity);
  if (!ptr.valid()) {
    delete ptr;
    return nullptr;
  }
  return ptr;
}

ConsentInfoInternalAndroid::ConsentInfoInternalAndroid(JNIEnv* jni_env,
                                                       jobject activity)
    : helper_(nullptr) {
  util::Initialize(jni_env, activity);

  const std::vector<firebase::internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(env, activity,
                               firebase::internal::EmbeddedFile::ToVector(
                                   firebase_gma::gma_resources_filename,
                                   firebase_gma::gma_resources_data,
                                   firebase_gma::gma_resources_size));
  if (!consent_info_helper::CacheClassFromFiles(env, activity,
                                                &embedded_files) != nullptr &&
      consent_info_helper::CacheMethodIds(env, activity)) {
    util::Terminate();
    return;
  }
  jobject helper_ref = env->NewObject(
      consent_info_helper::GetClass(),
      consent_info_helper::GetMethodId(rewarded_ad_helper::kConstructor),
      reinterpret_cast<jlong>(this));
  util::CheckAndClearJniExceptions(env);

  FIREBASE_ASSERT(helper_ref);
  helper_ = env->NewGlobalRef(helper_ref);
  FIREBASE_ASSERT(helper_);
  env->DeleteLocalRef(helper_ref);
}

ConsentInfoInternalAndroid::~ConsentInfoInternalAndroid() {
  env->DeleteGlobalRef(helper_);
  helper_ = nullptr;
  consent_info_helper::Terminate();
  util::Terminate();
}

Future<void> ConsentInfoInternalAndroid::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnRequestConsentInfoUpdate);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

ConsentStatus ConsentInfoInternalAndroid::GetConsentStatus() {
  return kConsentStatusUnknown;
}
ConsentFormStatus ConsentInfoInternalAndroid::GetConsentFormStatus() {
  return kConsentFormStatusUnknown;
}

Future<void> ConsentInfoInternalAndroid::LoadConsentForm() {
  SafeFutureHandle<void> handle = CreateFuture(kConsentInfoFnLoadConsentForm);

  CompleteFuture(handle, kConsentFormSuccess);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalAndroid::ShowConsentForm(FormParent parent) {
  SafeFutureHandle<void> handle = CreateFuture(kConsentInfoFnShowConsentForm);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalAndroid::LoadAndShowConsentFormIfRequired(
    FormParent parent) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnLoadAndShowConsentFormIfRequired);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalAndroid::GetPrivacyOptionsRequirementStatus() {
  return kPrivacyOptionsRequirementStatusUnknown;
}

Future<void> ConsentInfoInternalAndroid::ShowPrivacyOptionsForm(
    FormParent parent) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnShowPrivacyOptionsForm);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalAndroid::CanRequestAds() { return false; }

void ConsentInfoInternalAndroid::Reset() {}

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase
