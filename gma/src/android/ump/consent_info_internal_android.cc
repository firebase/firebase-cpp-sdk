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
#include "gma/ump_resources.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

// clang-format off
#define CONSENTINFOHELPER_METHODS(X)                                            \
  X(Constructor, "<init>", "(JLandroid/app/Activity;)V"),                       \
  X(GetConsentStatus, "getConsentStatus", "()I"),                               \
  X(RequestConsentStatusUpdate, "requestConsentStatusUpdate",                   \
    "(ZILjava/util/ArrayList;)V"),                                              \
  X(Disconnect, "disconnect", "()V")
// clang-format on

METHOD_LOOKUP_DECLARATION(consent_info_helper, CONSENTINFOHELPER_METHODS);
METHOD_LOOKUP_DEFINITION(
    consent_info_helper,
    "com/google/firebase/gma/internal/cpp/ConsentInfoHelper",
    CONSENTINFOHELPER_METHODS);

// clang-format off
#define CONSENTINFORMATION_CONSENTSTATUS_FIELDS(X)                              \
  X(Unknown, "UNKNOWN", "I", util::kFieldTypeStatic),                           \
  X(NotRequired, "NOT_REQUIRED", "I", util::kFieldTypeStatic),                  \
  X(Required, "REQUIRED", "I", util::kFieldTypeStatic),                         \
  X(Obtained, "OBTAINED", "I", util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(consentinformation_consentstatus, METHOD_LOOKUP_NONE,
                          CONSENTINFORMATION_CONSENTSTATUS_FIELDS);
METHOD_LOOKUP_DEFINITION(
    consentinformation_consentstatus,
    PROGUARD_KEEP_CLASS
    "com/google/android/ump/ConsentInformation$ConsentStatus",
    METHOD_LOOKUP_NONE, CONSENTINFORMATION_CONSENTSTATUS_FIELDS);

// clang-format off
#define FORMERROR_ERRORCODE_FIELDS(X)                                           \
  X(InternalError, "INTERNAL_ERROR", "I", util::kFieldTypeStatic),              \
  X(InternetError, "INTERNET_ERROR", "I", util::kFieldTypeStatic),              \
  X(InvalidOperation, "INVALID_OPERATION", "I", util::kFieldTypeStatic),        \
  X(TimeOut, "TIME_OUT", "I", util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(formerror_errorcode, METHOD_LOOKUP_NONE,
                          FORMERROR_ERRORCODE_FIELDS);
METHOD_LOOKUP_DEFINITION(formerror_errorcode,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/ump/FormError$ErrorCode",
                         METHOD_LOOKUP_NONE, FORMERROR_ERRORCODE_FIELDS);

// clang-format off
#define CONSENTDEBUGSETTINGS_DEBUGGEOGRAPHY_FIELDS(X)                           \
  X(Disabled, "DEBUG_GEOGRAPHY_DISABLED", "I", util::kFieldTypeStatic),         \
  X(EEA, "DEBUG_GEOGRAPHY_EEA", "I", util::kFieldTypeStatic),                   \
  X(NotEEA, "DEBUG_GEOGRAPHY_NOT_EEA", "I", util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(consentdebugsettings_debuggeography,
                          METHOD_LOOKUP_NONE,
                          CONSENTDEBUGSETTINGS_DEBUGGEOGRAPHY_FIELDS);
METHOD_LOOKUP_DEFINITION(
    consentdebugsettings_debuggeography,
    PROGUARD_KEEP_CLASS
    "com/google/android/ump/ConsentDebugSettings$DebugGeography",
    METHOD_LOOKUP_NONE, CONSENTDEBUGSETTINGS_DEBUGGEOGRAPHY_FIELDS);

// This explicitly implements the constructor for the outer class,
// ConsentInfoInternal.
ConsentInfoInternal* ConsentInfoInternal::CreateInstance(JNIEnv* jni_env,
                                                         jobject activity) {
  ConsentInfoInternalAndroid* ptr =
      new ConsentInfoInternalAndroid(jni_env, activity);
  if (!ptr->valid()) {
    delete ptr;
    return nullptr;
  }
  return ptr;
}

static void ReleaseClasses(JNIEnv* env) {
  consent_info_helper::ReleaseClass(env);
  consentinformation_consentstatus::ReleaseClass(env);
  formerror_errorcode::ReleaseClass(env);
  consentdebugsettings_debuggeography::ReleaseClass(env);
}

ConsentInfoInternalAndroid::~ConsentInfoInternalAndroid() {
  JNIEnv* env = GetJNIEnv();
  env->CallVoidMethod(helper_, consent_info_helper::GetMethodId(
                                   consent_info_helper::kDisconnect));
  env->DeleteGlobalRef(helper_);
  helper_ = nullptr;

  ReleaseClasses(env);
  util::Terminate(env);

  env->DeleteGlobalRef(activity_);
  activity_ = nullptr;
  java_vm_ = nullptr;
}

ConsentInfoInternalAndroid::ConsentInfoInternalAndroid(JNIEnv* env,
                                                       jobject activity)
    : java_vm_(nullptr), activity_(nullptr), helper_(nullptr) {
  util::Initialize(env, activity);
  env->GetJavaVM(&java_vm_);

  const std::vector<firebase::internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(env, activity,
                               firebase::internal::EmbeddedFile::ToVector(
                                   firebase_ump::ump_resources_filename,
                                   firebase_ump::ump_resources_data,
                                   firebase_ump::ump_resources_size));
  if (!(consent_info_helper::CacheClassFromFiles(env, activity,
                                                 &embedded_files) != nullptr &&
        consent_info_helper::CacheMethodIds(env, activity) &&
        consentinformation_consentstatus::CacheFieldIds(env, activity) &&
        formerror_errorcode::CacheFieldIds(env, activity) &&
        consentdebugsettings_debuggeography::CacheFieldIds(env, activity))) {
    ReleaseClasses(env);
    util::Terminate(env);
    return;
  }
  jobject helper_ref = env->NewObject(
      consent_info_helper::GetClass(),
      consent_info_helper::GetMethodId(consent_info_helper::kConstructor),
      reinterpret_cast<jlong>(this));
  util::CheckAndClearJniExceptions(env);
  if (!helper_ref) {
    ReleaseClasses(env);
    util::Terminate(env);
    return;
  }

  helper_ = env->NewGlobalRef(helper_ref);
  FIREBASE_ASSERT(helper_);
  env->DeleteLocalRef(helper_ref);

  activity_ = env->NewGlobalRef(activity);

  util::CheckAndClearJniExceptions(env);
}

// clang-format off
#define ENUM_VALUE(class_namespace, field_name)           \
   env->GetStaticIntField(class_namespace::GetClass(),    \
                          class_namespace::GetFieldId(class_namespace::k##field_name))
// clang-format on

static ConsentStatus CppConsentStatusFromAndroidConsentStatus(JNIEnv* env,
                                                              jint status) {
  static jint status_unknown =
      ENUM_VALUE(consentinformation_consentstatus, Unknown);
  static jint status_required =
      ENUM_VALUE(consentinformation_consentstatus, Required);
  static jint status_not_required =
      ENUM_VALUE(consentinformation_consentstatus, NotRequired);
  static jint status_obtained =
      ENUM_VALUE(consentinformation_consentstatus, Obtained);
  if (status == status_unknown) return kConsentStatusUnknown;
  if (status == status_required) return kConsentStatusRequired;
  if (status == status_not_required) return kConsentStatusNotRequired;
  if (status == status_obtained) return kConsentStatusObtained;
  return kConsentStatusUnknown;
}

static bool MessageContains(JNIEnv* env, jstring message, const char* text) {
  if (!message) return false;
  std::string message_str = util::JStringToString(env, message);
  return (message_str.find(text) != std::string::npos);
}

static jint AndroidDebugGeographyFromCppDebugGeography(
    JNIEnv* env, ConsentDebugGeography geo) {
  static jint geo_disabled =
      ENUM_VALUE(consentdebugsettings_debuggeography, Disabled);
  static jint geo_eea = ENUM_VALUE(consentdebugsettings_debuggeography, EEA);
  static jint geo_not_eea =
      ENUM_VALUE(consentdebugsettings_debuggeography, NotEEA);
  switch (geo) {
    case kConsentDebugGeographyDisabled:
      return geo_disabled;
    case kConsentDebugGeographyEEA:
      return geo_eea;
    case kConsentDebugGeographyNonEEA:
      return geo_not_eea;
    default:
      return geo_disabled;
  }
}

static ConsentRequestError CppConsentRequestErrorFromAndroidFormError(
    JNIEnv* env, jint error, jstring message = nullptr) {
  static jint error_success = 0;
  static jint error_internal = ENUM_VALUE(formerror_errorcode, InternalError);
  static jint error_network = ENUM_VALUE(formerror_errorcode, InternetError);
  static jint error_invalid_operation =
      ENUM_VALUE(formerror_errorcode, InvalidOperation);

  if (error == error_success) return kConsentRequestSuccess;
  if (error == error_internal) return kConsentRequestErrorInternal;
  if (error == error_network) return kConsentRequestErrorNetwork;
  if (error == error_invalid_operation)
    return kConsentRequestErrorInvalidOperation;
  return kConsentRequestErrorUnknown;
}

static ConsentFormError CppConsentFormErrorFromAndroidFormError(
    JNIEnv* env, jint error, jstring message = nullptr) {
  static jint error_success = 0;
  static jint error_internal = ENUM_VALUE(formerror_errorcode, InternalError);
  static jint error_invalid_operation =
      ENUM_VALUE(formerror_errorcode, InvalidOperation);
  static jint error_timeout = ENUM_VALUE(formerror_errorcode, TimeOut);
  if (error == error_success) return kConsentFormSuccess;
  if (error == error_internal) return kConsentFormErrorInternal;
  if (error == error_timeout) return kConsentFormErrorTimeout;
  if (error == error_invalid_operation) {
    if (MessageContains(env, message, "unavailable"))
      return kConsentFormErrorUnavailable;
    else
      return kConsentFormErrorInvalidOperation;
  }
  return kConsentFormErrorUnknown;
}

Future<void> ConsentInfoInternalAndroid::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnRequestConsentInfoUpdate);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

ConsentStatus ConsentInfoInternalAndroid::GetConsentStatus() {
  if (!valid()) {
    return kConsentStatusUnknown;
  }
  JNIEnv* env = GetJNIEnv();
  jint result = env->CallIntMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kGetConsentStatus));
  return CppConsentStatusFromAndroidConsentStatus(env, result);
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

JNIEnv* ConsentInfoInternalAndroid::GetJNIEnv() {
  return firebase::util::GetThreadsafeJNIEnv(java_vm_);
}
jobject ConsentInfoInternalAndroid::activity() { return activity_; }

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase
