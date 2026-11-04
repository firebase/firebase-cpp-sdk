// Copyright 2023 Google LLC

#include "ump/src/android/consent_info_internal_android.h"

#include <jni.h>
#include <strings.h>
#include <string>
#include <vector>

#include "app/src/assert.h"
#include "app/src/jni/collection.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
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
  X(Constructor, "<init>", "(JLandroid/app/Activity;)V")                        \
  X(GetConsentStatus, "getConsentStatus", "()I")                                \
  X(RequestConsentInfoUpdate, "requestConsentInfoUpdate",                       \
    "(JZILjava/util/ArrayList;)V")                                              \
  X(LoadConsentForm, "loadConsentForm", "(J)V")                                 \
  X(ShowConsentForm, "showConsentForm", "(JLandroid/app/Activity;)Z")           \
  X(LoadAndShowConsentFormIfRequired, "loadAndShowConsentFormIfRequired",       \
    "(JLandroid/app/Activity;)V")                                               \
  X(GetPrivacyOptionsRequirementStatus, "getPrivacyOptionsRequirementStatus",   \
    "()I")                                                                      \
  X(ShowPrivacyOptionsForm, "showPrivacyOptionsForm",                           \
    "(JLandroid/app/Activity;)V")                                               \
  X(Reset, "reset", "()V")                                                      \
  X(CanRequestAds, "canRequestAds", "()Z")                                      \
  X(IsConsentFormAvailable, "isConsentFormAvailable", "()Z")                    \
  X(Disconnect, "disconnect", "()V")

#define CONSENTINFOHELPER_FIELDS(X)                                             \
  X(PrivacyOptionsRequirementUnknown,                                           \
    "PRIVACY_OPTIONS_REQUIREMENT_UNKNOWN", "I", jni::kFieldTypeStatic)          \
  X(PrivacyOptionsRequirementRequired,                                          \
    "PRIVACY_OPTIONS_REQUIREMENT_REQUIRED", "I", jni::kFieldTypeStatic)         \
  X(PrivacyOptionsRequirementNotRequired,                                       \
    "PRIVACY_OPTIONS_REQUIREMENT_NOT_REQUIRED", "I", jni::kFieldTypeStatic)     \
  X(FunctionRequestConsentInfoUpdate,                                           \
    "FUNCTION_REQUEST_CONSENT_INFO_UPDATE", "I", jni::kFieldTypeStatic)         \
  X(FunctionLoadConsentForm,                                                    \
    "FUNCTION_LOAD_CONSENT_FORM", "I", jni::kFieldTypeStatic)                   \
  X(FunctionShowConsentForm,                                                    \
    "FUNCTION_SHOW_CONSENT_FORM", "I", jni::kFieldTypeStatic)                   \
  X(FunctionLoadAndShowConsentFormIfRequired,                                   \
    "FUNCTION_LOAD_AND_SHOW_CONSENT_FORM_IF_REQUIRED", "I",                     \
    jni::kFieldTypeStatic)                                                      \
  X(FunctionShowPrivacyOptionsForm,                                             \
    "FUNCTION_SHOW_PRIVACY_OPTIONS_FORM", "I", jni::kFieldTypeStatic)           \
  X(FunctionCount, "FUNCTION_COUNT", "I", jni::kFieldTypeStatic)
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    consent_info_helper, CONSENTINFOHELPER_METHODS, CONSENTINFOHELPER_FIELDS,
    "com/google/firebase/ump/internal/cpp/ConsentInfoHelper")

// clang-format off
#define CONSENTINFORMATION_CONSENTSTATUS_FIELDS(X)                              \
  X(Unknown, "UNKNOWN", "I", jni::kFieldTypeStatic)                             \
  X(NotRequired, "NOT_REQUIRED", "I", jni::kFieldTypeStatic)                    \
  X(Required, "REQUIRED", "I", jni::kFieldTypeStatic)                           \
  X(Obtained, "OBTAINED", "I", jni::kFieldTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(
    consentinformation_consentstatus, METHOD_LOOKUP_NONE,
    CONSENTINFORMATION_CONSENTSTATUS_FIELDS,
    "com/google/android/ump/ConsentInformation$ConsentStatus")

// clang-format off
#define FORMERROR_ERRORCODE_FIELDS(X)                                           \
  X(InternalError, "INTERNAL_ERROR", "I", jni::kFieldTypeStatic)                \
  X(InternetError, "INTERNET_ERROR", "I", jni::kFieldTypeStatic)                \
  X(InvalidOperation, "INVALID_OPERATION", "I", jni::kFieldTypeStatic)          \
  X(TimeOut, "TIME_OUT", "I", jni::kFieldTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(formerror_errorcode, METHOD_LOOKUP_NONE,
                              FORMERROR_ERRORCODE_FIELDS,
                              "com/google/android/ump/FormError$ErrorCode")

// clang-format off
#define CONSENTDEBUGSETTINGS_DEBUGGEOGRAPHY_FIELDS(X)                           \
  X(Disabled, "DEBUG_GEOGRAPHY_DISABLED", "I", jni::kFieldTypeStatic)           \
  X(EEA, "DEBUG_GEOGRAPHY_EEA", "I", jni::kFieldTypeStatic)                     \
  X(NotEEA, "DEBUG_GEOGRAPHY_NOT_EEA", "I", jni::kFieldTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(
    consentdebugsettings_debuggeography, METHOD_LOOKUP_NONE,
    CONSENTDEBUGSETTINGS_DEBUGGEOGRAPHY_FIELDS,
    "com/google/android/ump/ConsentDebugSettings$DebugGeography")

ConsentInfoInternal* ConsentInfoInternal::CreateInstance(JNIEnv* jni_env,
                                                         jobject activity) {
  if (!jni_env || !activity) return nullptr;
  auto* ptr = new ConsentInfoInternalAndroid(jni_env, activity);
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
  if (!java_vm_) return;
  JNIEnv* env = GetJNIEnv();
  if (env && helper_) {
    env->CallVoidMethod(helper_, consent_info_helper::GetMethodId(
                                     consent_info_helper::kDisconnect));
    jni::CheckAndClearException(env);
  }
  {
    MutexLock lock(s_instance_mutex);
    if (s_instance == this) s_instance = nullptr;
  }
  if (env) {
    if (helper_) {
      env->DeleteGlobalRef(helper_);
      helper_ = nullptr;
    }
    if (consent_info_helper::GetClass()) {
      env->UnregisterNatives(consent_info_helper::GetClass());
      jni::CheckAndClearException(env);
    }
    ReleaseClasses(env);
    util::Terminate(env);
  }
  java_vm_ = nullptr;
}

namespace {
bool ReadStaticIntField(JNIEnv* env, jclass clazz, jfieldID field_id,
                        jint* out) {
  if (!env || !clazz || !field_id || !out) return false;
  *out = env->GetStaticIntField(clazz, field_id);
  return !jni::CheckAndClearException(env);
}
}  // namespace

#define READ_ENUM_VALUE(ns, field, target)                                     \
  if (!ReadStaticIntField(env, ns::GetClass(), ns::GetFieldId(ns::k##field),   \
                          &(target)))                                          \
    return false;

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
    JNIEnv* env, jclass, jint future_fn, jlong ptr, jlong future_handle,
    jint error_code, jobject error_message_obj) {
  MutexLock lock(s_instance_mutex);
  auto* inst = reinterpret_cast<ConsentInfoInternalAndroid*>(ptr);
  if (!ptr || !s_instance || s_instance != inst) return;
  std::string msg =
      error_message_obj
          ? jni::JStringToString(env, static_cast<jstring>(error_message_obj))
          : "";
  inst->CompleteFutureFromJniCallback(
      env, future_fn, static_cast<FutureHandleId>(future_handle),
      static_cast<int>(error_code), !msg.empty() ? msg.c_str() : nullptr);
}

ConsentInfoInternalAndroid::ConsentInfoInternalAndroid(JNIEnv* env,
                                                       jobject activity) {
  if (!env || !activity) return;
  MutexLock lock(s_instance_mutex);
  FIREBASE_ASSERT(s_instance == nullptr);
  if (!util::Initialize(env, activity)) return;
  env->GetJavaVM(&java_vm_);

  {
    MutexLock ef_lock(g_cached_ump_embedded_files_mutex);
    if (!g_cached_ump_embedded_files) {
      g_cached_ump_embedded_files =
          new std::vector<firebase::internal::EmbeddedFile>(
              util::CacheEmbeddedFiles(
                  env, activity,
                  firebase::internal::EmbeddedFile::ToVector(
                      firebase_ump::ump_resources_filename,
                      firebase_ump::ump_resources_data,
                      firebase_ump::ump_resources_size)));
    }
  }
  auto fail = [&](bool unregister) {
    s_instance = nullptr;
    if (unregister && consent_info_helper::GetClass()) {
      env->UnregisterNatives(consent_info_helper::GetClass());
      jni::CheckAndClearException(env);
    }
    ReleaseClasses(env);
    util::Terminate(env);
    java_vm_ = nullptr;
  };

  if (!(consent_info_helper::CacheClassFromFiles(
            env, activity, g_cached_ump_embedded_files) != nullptr &&
        consent_info_helper::CacheMethodIds(env, activity) &&
        consent_info_helper::CacheFieldIds(env, activity) &&
        consentinformation_consentstatus::CacheFieldIds(env, activity) &&
        formerror_errorcode::CacheFieldIds(env, activity) &&
        consentdebugsettings_debuggeography::CacheFieldIds(env, activity))) {
    fail(false);
    return;
  }

  static const JNINativeMethod kNativeMethods[] = {
      {"completeFuture", "(IJJILjava/lang/String;)V",
       reinterpret_cast<void*>(&JNI_ConsentInfoHelper_completeFuture)}};
  if (env->RegisterNatives(consent_info_helper::GetClass(), kNativeMethods,
                           FIREBASE_ARRAYSIZE(kNativeMethods)) != 0 ||
      jni::CheckAndClearException(env) || !CacheEnumValues(env)) {
    fail(true);
    return;
  }

  set_activity_method_id_ = env->GetMethodID(
      consent_info_helper::GetClass(), "setActivity",
      "(Landroid/app/Activity;)V");
  if (jni::CheckAndClearException(env)) set_activity_method_id_ = nullptr;

  s_instance = this;
  jobject helper_ref = env->NewObject(
      consent_info_helper::GetClass(),
      consent_info_helper::GetMethodId(consent_info_helper::kConstructor),
      reinterpret_cast<jlong>(this), activity);
  if (!jni::CheckAndClearException(env) && helper_ref) {
    helper_ = env->NewGlobalRef(helper_ref);
    env->DeleteLocalRef(helper_ref);
  }
  if (jni::CheckAndClearException(env) || !helper_) fail(true);
}

void ConsentInfoInternalAndroid::SetActivity(JNIEnv* jni_env,
                                             jobject activity) {
  if (!valid() || !activity || !set_activity_method_id_) return;
  if (JNIEnv* env = jni_env ? jni_env : GetJNIEnv()) {
    env->CallVoidMethod(helper_, set_activity_method_id_, activity);
    jni::CheckAndClearException(env);
  }
}

ConsentStatus ConsentInfoInternalAndroid::CppConsentStatusFromAndroid(
    jint status) const {
  if (status == enums().consentstatus_unknown) return kConsentStatusUnknown;
  if (status == enums().consentstatus_required) return kConsentStatusRequired;
  if (status == enums().consentstatus_not_required)
    return kConsentStatusNotRequired;
  if (status == enums().consentstatus_obtained) return kConsentStatusObtained;
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
  return kPrivacyOptionsRequirementStatusUnknown;
}

jint ConsentInfoInternalAndroid::AndroidDebugGeographyFromCppDebugGeography(
    ConsentDebugGeography geo) const {
  switch (geo) {
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

ConsentRequestError
ConsentInfoInternalAndroid::CppConsentRequestErrorFromAndroidFormError(
    jint error, const char* message) const {
  if (error == enums().formerror_success) return kConsentRequestSuccess;
  if (error == enums().formerror_internal) return kConsentRequestErrorInternal;
  if (error == enums().formerror_network) return kConsentRequestErrorNetwork;
  if (error == enums().formerror_timeout) return kConsentRequestErrorTimeout;
  if (error == enums().formerror_invalid_operation) {
    if (message && strcasestr(message, "misconfiguration"))
      return kConsentRequestErrorMisconfiguration;
    if (message && strcasestr(message, "requires a valid application ID"))
      return kConsentRequestErrorInvalidAppId;
    return kConsentRequestErrorInvalidOperation;
  }
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
    if (message && (strcasestr(message, "no available form") ||
                    strcasestr(message, "form is not required")))
      return kConsentFormErrorUnavailable;
    if (message && strcasestr(message, "can only be invoked once"))
      return kConsentFormErrorAlreadyUsed;
    return kConsentFormErrorInvalidOperation;
  }
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
  jobject debug_ids =
      jni::StdVectorToJavaList(env, params.debug_settings.debug_device_ids);
  env->CallVoidMethod(
      helper_,
      consent_info_helper::GetMethodId(
          consent_info_helper::kRequestConsentInfoUpdate),
      static_cast<jlong>(handle.get().id()),
      params.tag_for_under_age_of_consent ? JNI_TRUE : JNI_FALSE,
      AndroidDebugGeographyFromCppDebugGeography(
          params.debug_settings.debug_geography),
      debug_ids);
  if (env->ExceptionCheck()) {
    CompleteFuture(handle, kConsentRequestErrorInternal,
                   jni::GetAndClearExceptionMessage(env).c_str());
  }
  if (debug_ids) env->DeleteLocalRef(debug_ids);
  return MakeFuture<void>(futures(), handle);
}

ConsentStatus ConsentInfoInternalAndroid::GetConsentStatus() {
  if (!valid()) return kConsentStatusUnknown;
  JNIEnv* env = GetJNIEnv();
  jint res = env->CallIntMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kGetConsentStatus));
  return jni::CheckAndClearException(env) ? kConsentStatusUnknown
                                          : CppConsentStatusFromAndroid(res);
}

ConsentFormStatus ConsentInfoInternalAndroid::GetConsentFormStatus() {
  if (!valid() || !has_requested_consent_info_update_.load()) {
    return kConsentFormStatusUnknown;
  }
  JNIEnv* env = GetJNIEnv();
  jboolean avail = env->CallBooleanMethod(
      helper_, consent_info_helper::GetMethodId(
                   consent_info_helper::kIsConsentFormAvailable));
  if (jni::CheckAndClearException(env)) return kConsentFormStatusUnknown;
  return avail == JNI_FALSE ? kConsentFormStatusUnavailable
                            : kConsentFormStatusAvailable;
}

Future<void> ConsentInfoInternalAndroid::RunFormMethod(ConsentInfoFn fn,
                                                       int method_id,
                                                       FormParent parent) {
  if (!valid()) return Future<void>();
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(fn, kConsentFormErrorOperationInProgress,
                                  &handle, &in_progress)) {
    return in_progress;
  }
  JNIEnv* env = GetJNIEnv();
  jlong jh = static_cast<jlong>(handle.get().id());
  auto mid = consent_info_helper::GetMethodId(
      static_cast<consent_info_helper::Method>(method_id));
  if (fn == kConsentInfoFnLoadConsentForm) {
    env->CallVoidMethod(helper_, mid, jh);
  } else if (fn == kConsentInfoFnShowConsentForm) {
    jboolean ok = env->CallBooleanMethod(helper_, mid, jh, parent);
    if (!env->ExceptionCheck() && ok == JNI_FALSE) {
      CompleteFuture(
          handle, kConsentFormErrorUnavailable,
          "The consent form is unavailable. Please call LoadConsentForm and "
          "ensure it completes successfully before calling ShowConsentForm.");
    }
  } else {
    env->CallVoidMethod(helper_, mid, jh, parent);
  }
  if (env->ExceptionCheck()) {
    CompleteFuture(handle, kConsentFormErrorInternal,
                   jni::GetAndClearExceptionMessage(env).c_str());
  }
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalAndroid::LoadConsentForm() {
  return RunFormMethod(kConsentInfoFnLoadConsentForm,
                       consent_info_helper::kLoadConsentForm, nullptr);
}

Future<void> ConsentInfoInternalAndroid::ShowConsentForm(FormParent parent) {
  return RunFormMethod(kConsentInfoFnShowConsentForm,
                       consent_info_helper::kShowConsentForm, parent);
}

Future<void> ConsentInfoInternalAndroid::LoadAndShowConsentFormIfRequired(
    FormParent parent) {
  return RunFormMethod(kConsentInfoFnLoadAndShowConsentFormIfRequired,
                       consent_info_helper::kLoadAndShowConsentFormIfRequired,
                       parent);
}

Future<void> ConsentInfoInternalAndroid::ShowPrivacyOptionsForm(
    FormParent parent) {
  return RunFormMethod(kConsentInfoFnShowPrivacyOptionsForm,
                       consent_info_helper::kShowPrivacyOptionsForm, parent);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalAndroid::GetPrivacyOptionsRequirementStatus() {
  if (!valid()) return kPrivacyOptionsRequirementStatusUnknown;
  JNIEnv* env = GetJNIEnv();
  jint res = env->CallIntMethod(
      helper_, consent_info_helper::GetMethodId(
                   consent_info_helper::kGetPrivacyOptionsRequirementStatus));
  return jni::CheckAndClearException(env)
             ? kPrivacyOptionsRequirementStatusUnknown
             : CppPrivacyOptionsRequirementStatusFromAndroid(res);
}

bool ConsentInfoInternalAndroid::CanRequestAds() {
  if (!valid()) return false;
  JNIEnv* env = GetJNIEnv();
  jboolean ok = env->CallBooleanMethod(
      helper_,
      consent_info_helper::GetMethodId(consent_info_helper::kCanRequestAds));
  return !jni::CheckAndClearException(env) && ok != JNI_FALSE;
}

void ConsentInfoInternalAndroid::Reset() {
  has_requested_consent_info_update_.store(false);
  if (!valid()) return;
  JNIEnv* env = GetJNIEnv();
  env->CallVoidMethod(
      helper_, consent_info_helper::GetMethodId(consent_info_helper::kReset));
  jni::CheckAndClearException(env);
}

JNIEnv* ConsentInfoInternalAndroid::GetJNIEnv() {
  return ::firebase::util::GetThreadsafeJNIEnv(java_vm_);
}

void ConsentInfoInternalAndroid::CompleteFutureFromJniCallback(
    JNIEnv*, jint future_fn, FutureHandleId handle_id, int java_error_code,
    const char* error_message) {
  if (future_fn < 0 || future_fn >= enums().function_count) return;
  {
    MutexLock lock(futures_mutex());
    if (!futures()->ValidFuture(handle_id)) return;
  }
  SafeFutureHandle<void> handle{FutureHandle(handle_id)};
  if (future_fn == enums().function_request_consent_info_update) {
    auto code = CppConsentRequestErrorFromAndroidFormError(java_error_code,
                                                           error_message);
    if (code == kConsentRequestSuccess) {
      has_requested_consent_info_update_.store(true);
    }
    CompleteFuture(handle, code, error_message);
  } else {
    CompleteFuture(handle,
                   CppConsentFormErrorFromAndroidFormError(java_error_code,
                                                           error_message),
                   error_message);
  }
}

}  // namespace internal
}  // namespace ump
}  // namespace firebase
