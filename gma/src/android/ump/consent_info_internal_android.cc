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

#include <jni.h>

#include <vector>

#include "app/src/assert.h"
#include "app/src/thread.h"
#include "app/src/util_android.h"
#include "firebase/internal/common.h"
#include "gma/ump_resources.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

ConsentInfoInternalAndroid* ConsentInfoInternalAndroid::s_instance = nullptr;
firebase::Mutex ConsentInfoInternalAndroid::s_instance_mutex;

// clang-format off
#define CONSENTINFOHELPER_METHODS(X)                                            \
  X(Constructor, "<init>", "(JLandroid/app/Activity;)V"),                       \
  X(GetConsentStatus, "getConsentStatus", "()I"),                               \
  X(RequestConsentInfoUpdate, "requestConsentInfoUpdate",                       \
    "(JZILjava/util/ArrayList;)V"),                                             \
  X(LoadConsentForm, "loadConsentForm", "(J)V"),                                \
  X(ShowConsentForm, "showConsentForm", "(JLandroid/app/Activity;)Z"),          \
  X(LoadAndShowConsentFormIfRequired, "loadAndShowConsentFormIfRequired",       \
    "(JLandroid/app/Activity;)V"),                                              \
  X(GetPrivacyOptionsRequirementStatus, "getPrivacyOptionsRequirementStatus",   \
    "()I"),                                                                     \
  X(ShowPrivacyOptionsForm, "showPrivacyOptionsForm",                           \
    "(JLandroid/app/Activity;)V"),                                              \
  X(Reset, "reset", "()V"),							\
  X(CanRequestAds, "canRequestAds", "()Z"),                                     \
  X(IsConsentFormAvailable, "isConsentFormAvailable", "()Z"),		        \
  X(Disconnect, "disconnect", "()V")
// clang-format on

// clang-format off
#define CONSENTINFOHELPER_FIELDS(X)                                             \
  X(PrivacyOptionsRequirementUnknown,                                           \
    "PRIVACY_OPTIONS_REQUIREMENT_UNKNOWN", "I", util::kFieldTypeStatic),	\
  X(PrivacyOptionsRequirementRequired,                                          \
    "PRIVACY_OPTIONS_REQUIREMENT_REQUIRED", "I", util::kFieldTypeStatic),	\
  X(PrivacyOptionsRequirementNotRequired,                                       \
    "PRIVACY_OPTIONS_REQUIREMENT_NOT_REQUIRED", "I", util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(consent_info_helper, CONSENTINFOHELPER_METHODS,
                          CONSENTINFOHELPER_FIELDS);

METHOD_LOOKUP_DEFINITION(
    consent_info_helper,
    "com/google/firebase/ump/internal/cpp/ConsentInfoHelper",
    CONSENTINFOHELPER_METHODS, CONSENTINFOHELPER_FIELDS);

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
  MutexLock lock(s_instance_mutex);
  s_instance = nullptr;

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

  jint debug_geography_disabled;
  jint debug_geography_eea;
  jint debug_geography_not_eea;

  jint privacy_options_requirement_unknown;
  jint privacy_options_requirement_required;
  jint privacy_options_requirement_not_required;
} g_enum_cache;

void ConsentInfoInternalAndroid::JNI_ConsentInfoHelper_completeFuture(
    JNIEnv* env, jclass clazz, jint future_fn, jlong consent_info_internal_ptr,
    jlong future_handle, jint error_code, jobject error_message_obj) {
  MutexLock lock(s_instance_mutex);
  if (consent_info_internal_ptr == 0 || s_instance == 0 || future_fn < 0 ||
      future_fn >= kConsentInfoFnCount) {
    // Calling this with a null pointer or invalid fn, or if there is no active
    // instance, is a no-op, so just return.
    return;
  }
  ConsentInfoInternalAndroid* instance =
      reinterpret_cast<ConsentInfoInternalAndroid*>(consent_info_internal_ptr);
  if (s_instance != instance) {
    // If the instance we were called with does not match the current
    // instance, a bad race condition has occurred (whereby while waiting for
    // the operation to complete, ConsentInfo was deleted and then recreated).
    // In that case, fully ignore this callback.
    return;
  }
  std::string error_message =
      error_message_obj ? util::JniStringToString(env, error_message_obj) : "";
  instance->CompleteFutureFromJniCallback(
      env, static_cast<ConsentInfoFn>(future_fn),
      static_cast<FutureHandleId>(future_handle), static_cast<int>(error_code),
      error_message.length() > 0 ? error_message.c_str() : nullptr);
}

ConsentInfoInternalAndroid::ConsentInfoInternalAndroid(JNIEnv* env,
                                                       jobject activity)
    : java_vm_(nullptr),
      activity_(nullptr),
      helper_(nullptr),
      has_requested_consent_info_update_(false) {
  MutexLock lock(s_instance_mutex);
  FIREBASE_ASSERT(s_instance == nullptr);
  s_instance = this;

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
        consent_info_helper::CacheFieldIds(env, activity) &&
        consentinformation_consentstatus::CacheFieldIds(env, activity) &&
        formerror_errorcode::CacheFieldIds(env, activity) &&
        consentdebugsettings_debuggeography::CacheFieldIds(env, activity))) {
    ReleaseClasses(env);
    util::Terminate(env);
    return;
  }
  static const JNINativeMethod kConsentInfoHelperNativeMethods[] = {
      {"completeFuture", "(IJJILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_ConsentInfoHelper_completeFuture)}};
  if (!consent_info_helper::RegisterNatives(
          env, kConsentInfoHelperNativeMethods,
          FIREBASE_ARRAYSIZE(kConsentInfoHelperNativeMethods))) {
    util::CheckAndClearJniExceptions(env);
    ReleaseClasses(env);
    util::Terminate(env);
    return;
  }
  util::CheckAndClearJniExceptions(env);
  jobject helper_ref = env->NewObject(
      consent_info_helper::GetClass(),
      consent_info_helper::GetMethodId(consent_info_helper::kConstructor),
      reinterpret_cast<jlong>(this),
      activity);
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

  // Cache enum values when the class loads, to avoid JNI lookups during
  // callbacks later on when converting enums between Android and C++ values.
  g_enum_cache.consentstatus_unknown =
      ENUM_VALUE(consentinformation_consentstatus, Unknown);
  g_enum_cache.consentstatus_required =
      ENUM_VALUE(consentinformation_consentstatus, Required);
  g_enum_cache.consentstatus_not_required =
      ENUM_VALUE(consentinformation_consentstatus, NotRequired);
  g_enum_cache.consentstatus_obtained =
      ENUM_VALUE(consentinformation_consentstatus, Obtained);

  g_enum_cache.debug_geography_disabled =
      ENUM_VALUE(consentdebugsettings_debuggeography, Disabled);
  g_enum_cache.debug_geography_eea =
      ENUM_VALUE(consentdebugsettings_debuggeography, EEA);
  g_enum_cache.debug_geography_not_eea =
      ENUM_VALUE(consentdebugsettings_debuggeography, NotEEA);

  g_enum_cache.formerror_success = 0;
  g_enum_cache.formerror_internal =
      ENUM_VALUE(formerror_errorcode, InternalError);
  g_enum_cache.formerror_network =
      ENUM_VALUE(formerror_errorcode, InternetError);
  g_enum_cache.formerror_invalid_operation =
      ENUM_VALUE(formerror_errorcode, InvalidOperation);
  g_enum_cache.formerror_timeout = ENUM_VALUE(formerror_errorcode, TimeOut);

  g_enum_cache.privacy_options_requirement_unknown =
      ENUM_VALUE(consent_info_helper, PrivacyOptionsRequirementUnknown);
  g_enum_cache.privacy_options_requirement_required =
      ENUM_VALUE(consent_info_helper, PrivacyOptionsRequirementRequired);
  g_enum_cache.privacy_options_requirement_not_required =
      ENUM_VALUE(consent_info_helper, PrivacyOptionsRequirementNotRequired);

  util::CheckAndClearJniExceptions(env);
}

static ConsentStatus CppConsentStatusFromAndroid(jint status) {
  if (status == g_enum_cache.consentstatus_unknown)
    return kConsentStatusUnknown;
  if (status == g_enum_cache.consentstatus_required)
    return kConsentStatusRequired;
  if (status == g_enum_cache.consentstatus_not_required)
    return kConsentStatusNotRequired;
  if (status == g_enum_cache.consentstatus_obtained)
    return kConsentStatusObtained;
  return kConsentStatusUnknown;
}

static PrivacyOptionsRequirementStatus
CppPrivacyOptionsRequirementStatusFromAndroid(jint status) {
  if (status == g_enum_cache.privacy_options_requirement_unknown)
    return kPrivacyOptionsRequirementStatusUnknown;
  if (status == g_enum_cache.privacy_options_requirement_required)
    return kPrivacyOptionsRequirementStatusRequired;
  if (status == g_enum_cache.privacy_options_requirement_not_required)
    return kPrivacyOptionsRequirementStatusNotRequired;
  return kPrivacyOptionsRequirementStatusUnknown;
}

static jint AndroidDebugGeographyFromCppDebugGeography(
    ConsentDebugGeography geo) {
  // Cache values the first time this function runs.
  switch (geo) {
    case kConsentDebugGeographyDisabled:
      return g_enum_cache.debug_geography_disabled;
    case kConsentDebugGeographyEEA:
      return g_enum_cache.debug_geography_eea;
    case kConsentDebugGeographyNonEEA:
      return g_enum_cache.debug_geography_not_eea;
    default:
      return g_enum_cache.debug_geography_disabled;
  }
}

static ConsentRequestError CppConsentRequestErrorFromAndroidFormError(
    jint error, const char* message = nullptr) {
  // Cache values the first time this function runs.
  if (error == g_enum_cache.formerror_success) return kConsentRequestSuccess;
  if (error == g_enum_cache.formerror_internal)
    return kConsentRequestErrorInternal;
  if (error == g_enum_cache.formerror_network)
    return kConsentRequestErrorNetwork;
  if (error == g_enum_cache.formerror_invalid_operation)
    return kConsentRequestErrorInvalidOperation;
  return kConsentRequestErrorUnknown;
}

static ConsentFormError CppConsentFormErrorFromAndroidFormError(
    jint error, const char* message = nullptr) {
  if (error == g_enum_cache.formerror_success) return kConsentFormSuccess;
  if (error == g_enum_cache.formerror_internal)
    return kConsentFormErrorInternal;
  if (error == g_enum_cache.formerror_timeout) return kConsentFormErrorTimeout;
  if (error == g_enum_cache.formerror_invalid_operation) {
    if (message && strcasestr(message, "unavailable") != nullptr)
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
  JNIEnv* env = GetJNIEnv();

  jlong future_handle = static_cast<jlong>(handle.get().id());
  jboolean tag_for_under_age_of_consent =
      params.tag_for_under_age_of_consent ? JNI_TRUE : JNI_FALSE;
  jint debug_geography = AndroidDebugGeographyFromCppDebugGeography(
      params.debug_settings.debug_geography);
  jobject debug_device_ids_list =
      util::StdVectorToJavaList(env, params.debug_settings.debug_device_ids);
  env->CallVoidMethod(helper_,
                      consent_info_helper::GetMethodId(
                          consent_info_helper::kRequestConsentInfoUpdate),
                      future_handle, tag_for_under_age_of_consent,
                      debug_geography, debug_device_ids_list);

  if (util::HasExceptionOccurred(env)) {
    std::string exception_message = util::GetAndClearExceptionMessage(env);
    CompleteFuture(handle, kConsentRequestErrorInternal,
                   exception_message.c_str());
  } else {
    has_requested_consent_info_update_ = true;
  }
  env->DeleteLocalRef(debug_device_ids_list);

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
  if (util::HasExceptionOccurred(env)) {
    util::CheckAndClearJniExceptions(env);
    return kConsentStatusUnknown;
  }
  return CppConsentStatusFromAndroid(result);
}

ConsentFormStatus ConsentInfoInternalAndroid::GetConsentFormStatus() {
  if (!valid() || !has_requested_consent_info_update_) {
    return kConsentFormStatusUnknown;
  }
  JNIEnv* env = GetJNIEnv();
  jboolean is_available = env->CallBooleanMethod(
      helper_, consent_info_helper::GetMethodId(
                   consent_info_helper::kIsConsentFormAvailable));
  if (util::HasExceptionOccurred(env)) {
    util::CheckAndClearJniExceptions(env);
    return kConsentFormStatusUnknown;
  }
  return (is_available == JNI_FALSE) ? kConsentFormStatusUnavailable
                                     : kConsentFormStatusAvailable;
}

Future<void> ConsentInfoInternalAndroid::LoadConsentForm() {
  SafeFutureHandle<void> handle = CreateFuture(kConsentInfoFnLoadConsentForm);
  JNIEnv* env = GetJNIEnv();
  jlong future_handle = static_cast<jlong>(handle.get().id());

  env->CallVoidMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kLoadConsentForm),
      future_handle);
  if (util::HasExceptionOccurred(env)) {
    std::string exception_message = util::GetAndClearExceptionMessage(env);
    CompleteFuture(handle, kConsentFormErrorInternal,
                   exception_message.c_str());
  }
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalAndroid::ShowConsentForm(FormParent parent) {
  SafeFutureHandle<void> handle = CreateFuture(kConsentInfoFnShowConsentForm);
  JNIEnv* env = GetJNIEnv();

  jlong future_handle = static_cast<jlong>(handle.get().id());
  jboolean success = env->CallBooleanMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kShowConsentForm),
      future_handle, parent);
  if (util::HasExceptionOccurred(env)) {
    std::string exception_message = util::GetAndClearExceptionMessage(env);
    CompleteFuture(handle, kConsentFormErrorInternal,
                   exception_message.c_str());
  } else if (success == JNI_FALSE) {
    CompleteFuture(
        handle, kConsentFormErrorUnavailable,
        "The consent form is unavailable. Please call LoadConsentForm and "
        "ensure it completes successfully before calling ShowConsentForm.");
  }
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalAndroid::LoadAndShowConsentFormIfRequired(
    FormParent parent) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnLoadAndShowConsentFormIfRequired);

  JNIEnv* env = GetJNIEnv();
  jlong future_handle = static_cast<jlong>(handle.get().id());

  env->CallVoidMethod(
      helper_,
      consent_info_helper::GetMethodId(
          consent_info_helper::kLoadAndShowConsentFormIfRequired),
      future_handle, parent);
  if (util::HasExceptionOccurred(env)) {
    std::string exception_message = util::GetAndClearExceptionMessage(env);
    CompleteFuture(handle, kConsentFormErrorInternal,
                   exception_message.c_str());
  }

  return MakeFuture<void>(futures(), handle);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalAndroid::GetPrivacyOptionsRequirementStatus() {
  if (!valid()) {
    return kPrivacyOptionsRequirementStatusUnknown;
  }
  JNIEnv* env = GetJNIEnv();
  jint result = env->CallIntMethod(
      helper_, consent_info_helper::GetMethodId(
                   consent_info_helper::kGetPrivacyOptionsRequirementStatus));
  return CppPrivacyOptionsRequirementStatusFromAndroid(result);
}

Future<void> ConsentInfoInternalAndroid::ShowPrivacyOptionsForm(
    FormParent parent) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnShowPrivacyOptionsForm);

  JNIEnv* env = GetJNIEnv();
  jlong future_handle = static_cast<jlong>(handle.get().id());

  env->CallVoidMethod(helper_,
                      consent_info_helper::GetMethodId(
                          consent_info_helper::kShowPrivacyOptionsForm),
                      future_handle, parent);
  if (util::HasExceptionOccurred(env)) {
    std::string exception_message = util::GetAndClearExceptionMessage(env);
    CompleteFuture(handle, kConsentFormErrorInternal,
                   exception_message.c_str());
  }

  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalAndroid::CanRequestAds() {
  JNIEnv* env = GetJNIEnv();
  jboolean can_request = env->CallBooleanMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kCanRequestAds));
  if (util::HasExceptionOccurred(env)) {
    util::CheckAndClearJniExceptions(env);
    return false;
  }
  return (can_request == JNI_FALSE) ? false : true;
}

void ConsentInfoInternalAndroid::Reset() {
  JNIEnv* env = GetJNIEnv();
  env->CallVoidMethod(
      helper_, consent_info_helper::GetMethodId(consent_info_helper::kReset));
  util::CheckAndClearJniExceptions(env);
}

JNIEnv* ConsentInfoInternalAndroid::GetJNIEnv() {
  return firebase::util::GetThreadsafeJNIEnv(java_vm_);
}
jobject ConsentInfoInternalAndroid::activity() { return activity_; }

void ConsentInfoInternalAndroid::CompleteFutureFromJniCallback(
    JNIEnv* env, ConsentInfoFn future_fn, FutureHandleId handle_id,
    int java_error_code, const char* error_message) {
  if (!futures()->ValidFuture(handle_id)) {
    // This future is no longer valid, so no need to complete it.
    return;
  }
  FutureHandle raw_handle(handle_id);
  SafeFutureHandle<void> handle(raw_handle);
  if (future_fn == kConsentInfoFnRequestConsentInfoUpdate) {
    // RequestConsentInfoUpdate uses the ConsentRequestError enum.
    ConsentRequestError error_code = CppConsentRequestErrorFromAndroidFormError(
        java_error_code, error_message);
    if (error_message == nullptr) {
      error_message = GetConsentRequestErrorMessage(error_code);
    }
    CompleteFuture(handle, error_code, error_message);
  } else {
    // All other methods use the ConsentFormError enum.
    ConsentFormError error_code =
        CppConsentFormErrorFromAndroidFormError(java_error_code, error_message);
    if (error_message == nullptr) {
      error_message = GetConsentFormErrorMessage(error_code);
    }
    CompleteFuture(handle, error_code, error_message);
  }
}

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase
