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

// clang-format off
#define ENUM_VALUE(class_namespace, field_name)           \
   env->GetStaticIntField(class_namespace::GetClass(),    \
                          class_namespace::GetFieldId(class_namespace::k##field_name))
// clang-format on

static struct {
  jint consentstatus_unknown;
  jint consentstatus_required;
  jint consentstatus_not_required;
  jint consentstatus_obtained;

  jint formerror_success;
  jint formerror_internal;
  jint formerror_network;
  jint formerror_invalid_operation;
  jint formerror_timeout;

  jint debuggeo_disabled;
  jint debuggeo_eea;
  jint debuggeo_not_eea;
} g_enum_cache;

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

  // Cache enum values when the class loads.
  g_enum_cache.consentstatus_unknown =
      ENUM_VALUE(consentinformation_consentstatus, Unknown);
  g_enum_cache.consentstatus_required =
      ENUM_VALUE(consentinformation_consentstatus, Required);
  g_enum_cache.consentstatus_not_required =
      ENUM_VALUE(consentinformation_consentstatus, NotRequired);
  g_enum_cache.consentstatus_obtained =
      ENUM_VALUE(consentinformation_consentstatus, Obtained);

  g_enum_cache.debuggeo_disabled =
    ENUM_VALUE(consentdebugsettings_debuggeography, Disabled);
  g_enum_cache.debuggeo_eea =
    ENUM_VALUE(consentdebugsettings_debuggeography, EEA);
  g_enum_cache.debuggeo_not_eea =
    ENUM_VALUE(consentdebugsettings_debuggeography, NotEEA);

  g_enum_cache.formerror_success = 0;
  g_enum_cache.formerror_internal = ENUM_VALUE(formerror_errorcode, InternalError);
  g_enum_cache.formerror_network = ENUM_VALUE(formerror_errorcode, InternetError);
  g_enum_cache.formerror_invalid_operation =
    ENUM_VALUE(formerror_errorcode, InvalidOperation);
  g_enum_cache.formerror_timeout = ENUM_VALUE(formerror_errorcode, TimeOut);
  
  util::CheckAndClearJniExceptions(env);
}

static ConsentStatus CppConsentStatusFromAndroidConsentStatus(JNIEnv* env,
                                                              jint status) {
  if (status == g_enum_cache.consentstatus_unknown) return kConsentStatusUnknown;
  if (status == g_enum_cache.consentstatus_required) return kConsentStatusRequired;
  if (status == g_enum_cache.consentstatus_not_required) return kConsentStatusNotRequired;
  if (status == g_enum_cache.consentstatus_obtained) return kConsentStatusObtained;
  return kConsentStatusUnknown;
}

static bool MessageContains(JNIEnv* env, jstring message, const char* text) {
  if (!message) return false;
  std::string message_str = util::JStringToString(env, message);
  return (message_str.find(text) != std::string::npos);
}

static jint AndroidDebugGeographyFromCppDebugGeography(
    JNIEnv* env, ConsentDebugGeography geo) {
  // Cache values the first time this function runs.
  switch (geo) {
    case kConsentDebugGeographyDisabled:
      return g_enum_cache.debuggeo_disabled;
    case kConsentDebugGeographyEEA:
      return g_enum_cache.debuggeo_eea;
    case kConsentDebugGeographyNonEEA:
      return g_enum_cache.debuggeo_not_eea;
    default:
      return g_enum_cache.debuggeo_disabled;
  }
}

static ConsentRequestError CppConsentRequestErrorFromAndroidFormError(
    JNIEnv* env, jint error, jstring message = nullptr) {
  // Cache values the first time this function runs.
  if (error == g_enum_cache.formerror_success) return kConsentRequestSuccess;
  if (error == g_enum_cache.formerror_internal) return kConsentRequestErrorInternal;
  if (error == g_enum_cache.formerror_network) return kConsentRequestErrorNetwork;
  if (error == g_enum_cache.formerror_invalid_operation)
    return kConsentRequestErrorInvalidOperation;
  return kConsentRequestErrorUnknown;
}

static ConsentFormError CppConsentFormErrorFromAndroidFormError(
    JNIEnv* env, jint error, jstring message = nullptr) {
  if (error == g_enum_cache.formerror_success) return kConsentFormSuccess;
  if (error == g_enum_cache.formerror_internal) return kConsentFormErrorInternal;
  if (error == g_enum_cache.formerror_timeout) return kConsentFormErrorTimeout;
  if (error == g_enum_cache.formerror_invalid_operation) {
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
