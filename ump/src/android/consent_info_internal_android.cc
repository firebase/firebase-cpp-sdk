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

#include "ump/src/android/consent_info_internal_android.h"

#include <jni.h>
#include <strings.h>

#include <cassert>
#include <string>
#include <vector>

#include "app/src/assert.h"
#include "app/src/thread.h"
#include "app/src/util_android.h"
#include "firebase/internal/common.h"
#include "ump/ump_resources.h"

namespace firebase {
namespace ump {
namespace internal {

ConsentInfoInternalAndroid* ConsentInfoInternalAndroid::s_instance = nullptr;
firebase::Mutex ConsentInfoInternalAndroid::s_instance_mutex;

::firebase::Mutex g_cached_ump_embedded_files_mutex;
std::vector<::firebase::internal::EmbeddedFile>* g_cached_ump_embedded_files =
    nullptr;

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
  X(Reset, "reset", "()V"),                                                     \
  X(CanRequestAds, "canRequestAds", "()Z"),                                     \
  X(IsConsentFormAvailable, "isConsentFormAvailable", "()Z"),                   \
  X(Disconnect, "disconnect", "()V")
// clang-format on

// clang-format off
#define CONSENTINFOHELPER_FIELDS(X)                                             \
  X(PrivacyOptionsRequirementUnknown,                                           \
    "PRIVACY_OPTIONS_REQUIREMENT_UNKNOWN", "I", util::kFieldTypeStatic),        \
  X(PrivacyOptionsRequirementRequired,                                          \
    "PRIVACY_OPTIONS_REQUIREMENT_REQUIRED", "I", util::kFieldTypeStatic),       \
  X(PrivacyOptionsRequirementNotRequired,                                       \
    "PRIVACY_OPTIONS_REQUIREMENT_NOT_REQUIRED", "I", util::kFieldTypeStatic),   \
  X(FunctionRequestConsentInfoUpdate,                                           \
     "FUNCTION_REQUEST_CONSENT_INFO_UPDATE", "I", util::kFieldTypeStatic),       \
  X(FunctionLoadConsentForm,                                                    \
    "FUNCTION_LOAD_CONSENT_FORM", "I", util::kFieldTypeStatic),                 \
  X(FunctionShowConsentForm,                                                    \
    "FUNCTION_SHOW_CONSENT_FORM", "I", util::kFieldTypeStatic),                 \
  X(FunctionLoadAndShowConsentFormIfRequired,                                   \
    "FUNCTION_LOAD_AND_SHOW_CONSENT_FORM_IF_REQUIRED",                          \
    "I", util::kFieldTypeStatic),                                               \
  X(FunctionShowPrivacyOptionsForm,                                             \
    "FUNCTION_SHOW_PRIVACY_OPTIONS_FORM", "I", util::kFieldTypeStatic),         \
  X(FunctionCount, "FUNCTION_COUNT", "I", util::kFieldTypeStatic)
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

ConsentInfoInternal* ConsentInfoInternal::CreateInstance(JNIEnv* jni_env,
                                                         jobject activity) {
  if (!jni_env || !activity) {
    return nullptr;
  }
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
  if (!java_vm_) {
    return;
  }
  JNIEnv* env = GetJNIEnv();
  if (env && helper_) {
    env->CallVoidMethod(helper_, consent_info_helper::GetMethodId(
                                     consent_info_helper::kDisconnect));
    util::CheckAndClearJniExceptions(env);
  }

  {
    MutexLock lock(s_instance_mutex);
    if (s_instance == this) {
      s_instance = nullptr;
    }
  }

  if (env) {
    if (helper_) {
      env->DeleteGlobalRef(helper_);
      helper_ = nullptr;
    }
    if (consent_info_helper::GetClass() != nullptr) {
      env->UnregisterNatives(consent_info_helper::GetClass());
      util::CheckAndClearJniExceptions(env);
    }
    ReleaseClasses(env);
    util::Terminate(env);
  }
  java_vm_ = nullptr;
}

namespace {
bool ReadStaticIntField(JNIEnv* env, jclass clazz, jfieldID field_id,
                        jint* out_value) {
  if (!env || !clazz || !field_id || !out_value) {
    return false;
  }
  *out_value = env->GetStaticIntField(clazz, field_id);
  return !util::CheckAndClearJniExceptions(env);
}
}  // namespace

#define READ_ENUM_VALUE(class_namespace, field_name, target)               \
  do {                                                                     \
    if (!ReadStaticIntField(                                               \
            env, class_namespace::GetClass(),                              \
            class_namespace::GetFieldId(class_namespace::k##field_name),   \
            &(target))) {                                                  \
      return false;                                                        \
    }                                                                      \
  } while (0)

bool ConsentInfoInternalAndroid::CacheEnumValues(JNIEnv* env) {
  READ_ENUM_VALUE(consentinformation_consentstatus, Unknown,
                  enums_.consentstatus_unknown);
  READ_ENUM_VALUE(consentinformation_consentstatus, Required,
                  enums_.consentstatus_required);
  READ_ENUM_VALUE(consentinformation_consentstatus, NotRequired,
                  enums_.consentstatus_not_required);
  READ_ENUM_VALUE(consentinformation_consentstatus, Obtained,
                  enums_.consentstatus_obtained);

  READ_ENUM_VALUE(consentdebugsettings_debuggeography, Disabled,
                  enums_.debug_geography_disabled);
  READ_ENUM_VALUE(consentdebugsettings_debuggeography, EEA,
                  enums_.debug_geography_eea);
  READ_ENUM_VALUE(consentdebugsettings_debuggeography, NotEEA,
                  enums_.debug_geography_not_eea);
  enums_.debug_geography_regulated_us_state = 3;
  enums_.debug_geography_other = 4;

  enums_.formerror_success = 0;
  READ_ENUM_VALUE(formerror_errorcode, InternalError,
                  enums_.formerror_internal);
  READ_ENUM_VALUE(formerror_errorcode, InternetError, enums_.formerror_network);
  READ_ENUM_VALUE(formerror_errorcode, InvalidOperation,
                  enums_.formerror_invalid_operation);
  READ_ENUM_VALUE(formerror_errorcode, TimeOut, enums_.formerror_timeout);

  READ_ENUM_VALUE(consent_info_helper, PrivacyOptionsRequirementUnknown,
                  enums_.privacy_options_requirement_unknown);
  READ_ENUM_VALUE(consent_info_helper, PrivacyOptionsRequirementRequired,
                  enums_.privacy_options_requirement_required);
  READ_ENUM_VALUE(consent_info_helper, PrivacyOptionsRequirementNotRequired,
                  enums_.privacy_options_requirement_not_required);

  READ_ENUM_VALUE(consent_info_helper, FunctionRequestConsentInfoUpdate,
                  enums_.function_request_consent_info_update);
  READ_ENUM_VALUE(consent_info_helper, FunctionLoadConsentForm,
                  enums_.function_load_consent_form);
  READ_ENUM_VALUE(consent_info_helper, FunctionShowConsentForm,
                  enums_.function_show_consent_form);
  READ_ENUM_VALUE(consent_info_helper,
                  FunctionLoadAndShowConsentFormIfRequired,
                  enums_.function_load_and_show_consent_form_if_required);
  READ_ENUM_VALUE(consent_info_helper, FunctionShowPrivacyOptionsForm,
                  enums_.function_show_privacy_options_form);
  READ_ENUM_VALUE(consent_info_helper, FunctionCount, enums_.function_count);
  return true;
}

#undef READ_ENUM_VALUE

void ConsentInfoInternalAndroid::JNI_ConsentInfoHelper_completeFuture(
    JNIEnv* env, jclass clazz, jint future_fn, jlong consent_info_internal_ptr,
    jlong future_handle, jint error_code, jobject error_message_obj) {
  (void)clazz;
  MutexLock lock(s_instance_mutex);
  if (consent_info_internal_ptr == 0 || s_instance == nullptr) {
    return;
  }
  ConsentInfoInternalAndroid* instance =
      reinterpret_cast<ConsentInfoInternalAndroid*>(consent_info_internal_ptr);
  if (s_instance != instance) {
    return;
  }
  std::string error_message =
      error_message_obj
          ? util::JStringToString(env, static_cast<jstring>(error_message_obj))
          : "";
  instance->CompleteFutureFromJniCallback(
      env, future_fn, static_cast<FutureHandleId>(future_handle),
      static_cast<int>(error_code),
      !error_message.empty() ? error_message.c_str() : nullptr);
}

ConsentInfoInternalAndroid::ConsentInfoInternalAndroid(JNIEnv* env,
                                                       jobject activity)
    : java_vm_(nullptr),
      helper_(nullptr),
      set_activity_method_id_(nullptr),
      has_requested_consent_info_update_(false) {
  if (!env || !activity) {
    return;
  }
  MutexLock lock(s_instance_mutex);
  FIREBASE_ASSERT(s_instance == nullptr);

  if (!util::Initialize(env, activity)) {
    return;
  }
  env->GetJavaVM(&java_vm_);

  {
    MutexLock files_lock(
        ::firebase::ump::internal::g_cached_ump_embedded_files_mutex);
    if (::firebase::ump::internal::g_cached_ump_embedded_files == nullptr) {
      ::firebase::ump::internal::g_cached_ump_embedded_files =
          new std::vector<firebase::internal::EmbeddedFile>(
              util::CacheEmbeddedFiles(
                  env, activity,
                  firebase::internal::EmbeddedFile::ToVector(
                      firebase_ump::ump_resources_filename,
                      firebase_ump::ump_resources_data,
                      firebase_ump::ump_resources_size)));
    }
  }
  const std::vector<firebase::internal::EmbeddedFile>& embedded_files =
      *::firebase::ump::internal::g_cached_ump_embedded_files;

  if (!(consent_info_helper::CacheClassFromFiles(env, activity,
                                                 &embedded_files) != nullptr &&
        consent_info_helper::CacheMethodIds(env, activity) &&
        consent_info_helper::CacheFieldIds(env, activity) &&
        consentinformation_consentstatus::CacheFieldIds(env, activity) &&
        formerror_errorcode::CacheFieldIds(env, activity) &&
        consentdebugsettings_debuggeography::CacheFieldIds(env, activity))) {
    ReleaseClasses(env);
    util::Terminate(env);
    java_vm_ = nullptr;
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
    java_vm_ = nullptr;
    return;
  }
  util::CheckAndClearJniExceptions(env);

  if (!CacheEnumValues(env)) {
    env->UnregisterNatives(consent_info_helper::GetClass());
    util::CheckAndClearJniExceptions(env);
    ReleaseClasses(env);
    util::Terminate(env);
    java_vm_ = nullptr;
    return;
  }

  // Optional lookup of setActivity(Activity) if present in the embedded DEX.
  set_activity_method_id_ = env->GetMethodID(
      consent_info_helper::GetClass(), "setActivity",
      "(Landroid/app/Activity;)V");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    set_activity_method_id_ = nullptr;
  }

  // Publish s_instance before constructing ConsentInfoHelper in case an older
  // embedded DEX invokes completeFuture(-1, ...) inside its constructor.
  s_instance = this;

  jobject helper_ref = env->NewObject(
      consent_info_helper::GetClass(),
      consent_info_helper::GetMethodId(consent_info_helper::kConstructor),
      reinterpret_cast<jlong>(this), activity);
  if (util::CheckAndClearJniExceptions(env) || !helper_ref) {
    s_instance = nullptr;
    env->UnregisterNatives(consent_info_helper::GetClass());
    util::CheckAndClearJniExceptions(env);
    ReleaseClasses(env);
    util::Terminate(env);
    java_vm_ = nullptr;
    return;
  }

  helper_ = env->NewGlobalRef(helper_ref);
  env->DeleteLocalRef(helper_ref);
  if (util::CheckAndClearJniExceptions(env) || !helper_) {
    s_instance = nullptr;
    env->UnregisterNatives(consent_info_helper::GetClass());
    util::CheckAndClearJniExceptions(env);
    ReleaseClasses(env);
    util::Terminate(env);
    java_vm_ = nullptr;
    return;
  }
}

void ConsentInfoInternalAndroid::SetActivity(JNIEnv* jni_env,
                                             jobject activity) {
  if (!valid() || !activity || !set_activity_method_id_) {
    return;
  }
  JNIEnv* env = jni_env ? jni_env : GetJNIEnv();
  if (!env) {
    return;
  }
  env->CallVoidMethod(helper_, set_activity_method_id_, activity);
  util::CheckAndClearJniExceptions(env);
}

ConsentStatus ConsentInfoInternalAndroid::CppConsentStatusFromAndroid(
    jint status) const {
  if (status == enums().consentstatus_unknown) return kConsentStatusUnknown;
  if (status == enums().consentstatus_required) return kConsentStatusRequired;
  if (status == enums().consentstatus_not_required)
    return kConsentStatusNotRequired;
  if (status == enums().consentstatus_obtained) return kConsentStatusObtained;
  LogWarning("UMP: Unknown ConsentStatus returned by UMP Android SDK: %d",
             static_cast<int>(status));
  return kConsentStatusUnknown;
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalAndroid::CppPrivacyOptionsRequirementStatusFromAndroid(
    jint status) const {
  if (status == enums().privacy_options_requirement_unknown)
    return kPrivacyOptionsRequirementStatusUnknown;
  if (status == enums().privacy_options_requirement_required)
    return kPrivacyOptionsRequirementStatusRequired;
  if (status == enums().privacy_options_requirement_not_required)
    return kPrivacyOptionsRequirementStatusNotRequired;
  LogWarning(
      "UMP: Unknown PrivacyOptionsRequirementStatus returned by UMP Android "
      "SDK: %d",
      static_cast<int>(status));
  return kPrivacyOptionsRequirementStatusUnknown;
}

jint ConsentInfoInternalAndroid::AndroidDebugGeographyFromCppDebugGeography(
    ConsentDebugGeography geo) const {
  switch (geo) {
    case kConsentDebugGeographyDisabled:
      return enums().debug_geography_disabled;
    case kConsentDebugGeographyEEA:
      return enums().debug_geography_eea;
    case kConsentDebugGeographyNonEEA:
      return enums().debug_geography_not_eea;
    case kConsentDebugGeographyRegulatedUSState:
      return enums().debug_geography_regulated_us_state;
    case kConsentDebugGeographyOther:
      return enums().debug_geography_other;
    default:
      return enums().debug_geography_disabled;
  }
}

// Android uses FormError to report request errors as well.
ConsentRequestError
ConsentInfoInternalAndroid::CppConsentRequestErrorFromAndroidFormError(
    jint error, const char* message) const {
  if (error == enums().formerror_success) return kConsentRequestSuccess;
  if (error == enums().formerror_internal) return kConsentRequestErrorInternal;
  if (error == enums().formerror_network) return kConsentRequestErrorNetwork;
  if (error == enums().formerror_timeout) return kConsentRequestErrorTimeout;
  if (error == enums().formerror_invalid_operation) {
    if (message && strcasestr(message, "misconfiguration") != nullptr)
      return kConsentRequestErrorMisconfiguration;
    if (message &&
        strcasestr(message, "requires a valid application ID") != nullptr)
      return kConsentRequestErrorInvalidAppId;
    return kConsentRequestErrorInvalidOperation;
  }
  LogWarning("UMP: Unknown RequestError returned by UMP Android SDK: %d (%s)",
             static_cast<int>(error), message ? message : "");
  return kConsentRequestErrorUnknown;
}

ConsentFormError
ConsentInfoInternalAndroid::CppConsentFormErrorFromAndroidFormError(
    jint error, const char* message) const {
  if (error == enums().formerror_success) return kConsentFormSuccess;
  if (error == enums().formerror_internal) return kConsentFormErrorInternal;
  if (error == enums().formerror_network) return kConsentFormErrorNetwork;
  if (error == enums().formerror_timeout) return kConsentFormErrorTimeout;
  if (error == enums().formerror_invalid_operation) {
    if (message && strcasestr(message, "no available form") != nullptr)
      return kConsentFormErrorUnavailable;
    if (message && strcasestr(message, "form is not required") != nullptr)
      return kConsentFormErrorUnavailable;
    if (message && strcasestr(message, "can only be invoked once") != nullptr)
      return kConsentFormErrorAlreadyUsed;
    return kConsentFormErrorInvalidOperation;
  }
  LogWarning("UMP: Unknown FormError returned by UMP Android SDK: %d (%s)",
             static_cast<int>(error), message ? message : "");
  return kConsentFormErrorUnknown;
}

Future<void> ConsentInfoInternalAndroid::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  if (!valid()) return Future<void>();

  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnRequestConsentInfoUpdate,
                                  kConsentRequestErrorOperationInProgress,
                                  &handle, &in_progress)) {
    return in_progress;
  }

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

  if (env->ExceptionCheck()) {
    std::string exception_message = util::GetAndClearExceptionMessage(env);
    CompleteFuture(handle, kConsentRequestErrorInternal,
                   exception_message.c_str());
  }
  if (debug_device_ids_list) {
    env->DeleteLocalRef(debug_device_ids_list);
  }

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
  if (util::CheckAndClearJniExceptions(env)) {
    return kConsentStatusUnknown;
  }
  return CppConsentStatusFromAndroid(result);
}

ConsentFormStatus ConsentInfoInternalAndroid::GetConsentFormStatus() {
  if (!valid() || !has_requested_consent_info_update_.load()) {
    return kConsentFormStatusUnknown;
  }
  JNIEnv* env = GetJNIEnv();
  jboolean is_available = env->CallBooleanMethod(
      helper_, consent_info_helper::GetMethodId(
                   consent_info_helper::kIsConsentFormAvailable));
  if (util::CheckAndClearJniExceptions(env)) {
    return kConsentFormStatusUnknown;
  }
  return (is_available == JNI_FALSE) ? kConsentFormStatusUnavailable
                                     : kConsentFormStatusAvailable;
}

Future<void> ConsentInfoInternalAndroid::LoadConsentForm() {
  if (!valid()) return Future<void>();

  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnLoadConsentForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }

  JNIEnv* env = GetJNIEnv();
  jlong future_handle = static_cast<jlong>(handle.get().id());

  env->CallVoidMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kLoadConsentForm),
      future_handle);
  if (env->ExceptionCheck()) {
    std::string exception_message = util::GetAndClearExceptionMessage(env);
    CompleteFuture(handle, kConsentFormErrorInternal,
                   exception_message.c_str());
  }
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalAndroid::ShowConsentForm(FormParent parent) {
  if (!valid()) return Future<void>();

  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnShowConsentForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }

  JNIEnv* env = GetJNIEnv();
  jlong future_handle = static_cast<jlong>(handle.get().id());
  jboolean success = env->CallBooleanMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kShowConsentForm),
      future_handle, parent);
  if (env->ExceptionCheck()) {
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
  if (!valid()) return Future<void>();

  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(
          kConsentInfoFnLoadAndShowConsentFormIfRequired,
          kConsentFormErrorOperationInProgress, &handle, &in_progress)) {
    return in_progress;
  }

  JNIEnv* env = GetJNIEnv();
  jlong future_handle = static_cast<jlong>(handle.get().id());

  env->CallVoidMethod(
      helper_,
      consent_info_helper::GetMethodId(
          consent_info_helper::kLoadAndShowConsentFormIfRequired),
      future_handle, parent);
  if (env->ExceptionCheck()) {
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
  if (util::CheckAndClearJniExceptions(env)) {
    return kPrivacyOptionsRequirementStatusUnknown;
  }
  return CppPrivacyOptionsRequirementStatusFromAndroid(result);
}

Future<void> ConsentInfoInternalAndroid::ShowPrivacyOptionsForm(
    FormParent parent) {
  if (!valid()) return Future<void>();

  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnShowPrivacyOptionsForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }

  JNIEnv* env = GetJNIEnv();
  jlong future_handle = static_cast<jlong>(handle.get().id());

  env->CallVoidMethod(helper_,
                      consent_info_helper::GetMethodId(
                          consent_info_helper::kShowPrivacyOptionsForm),
                      future_handle, parent);
  if (env->ExceptionCheck()) {
    std::string exception_message = util::GetAndClearExceptionMessage(env);
    CompleteFuture(handle, kConsentFormErrorInternal,
                   exception_message.c_str());
  }

  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalAndroid::CanRequestAds() {
  if (!valid()) {
    return false;
  }
  JNIEnv* env = GetJNIEnv();
  jboolean can_request = env->CallBooleanMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kCanRequestAds));
  if (util::CheckAndClearJniExceptions(env)) {
    return false;
  }
  return (can_request != JNI_FALSE);
}

void ConsentInfoInternalAndroid::Reset() {
  has_requested_consent_info_update_.store(false);
  if (!valid()) {
    return;
  }
  JNIEnv* env = GetJNIEnv();
  env->CallVoidMethod(
      helper_, consent_info_helper::GetMethodId(consent_info_helper::kReset));
  util::CheckAndClearJniExceptions(env);
}

JNIEnv* ConsentInfoInternalAndroid::GetJNIEnv() {
  return firebase::util::GetThreadsafeJNIEnv(java_vm_);
}

void ConsentInfoInternalAndroid::CompleteFutureFromJniCallback(
    JNIEnv* env, jint future_fn, FutureHandleId handle_id, int java_error_code,
    const char* error_message) {
  (void)env;
  if (future_fn < 0 || future_fn >= enums().function_count) {
    return;
  }
  {
    MutexLock lock(futures_mutex());
    if (!futures()->ValidFuture(handle_id)) {
      return;
    }
  }
  FutureHandle raw_handle(handle_id);
  SafeFutureHandle<void> handle(raw_handle);
  if (future_fn == enums().function_request_consent_info_update) {
    ConsentRequestError error_code = CppConsentRequestErrorFromAndroidFormError(
        java_error_code, error_message);
    if (error_code == kConsentRequestSuccess) {
      has_requested_consent_info_update_.store(true);
    }
    CompleteFuture(handle, error_code, error_message);
  } else {
    ConsentFormError error_code =
        CppConsentFormErrorFromAndroidFormError(java_error_code, error_message);
    CompleteFuture(handle, error_code, error_message);
  }
}

}  // namespace internal
}  // namespace ump
}  // namespace firebase
