// Copyright 2023 Google LLC

#ifndef FIREBASE_UMP_SRC_ANDROID_CONSENT_INFO_INTERNAL_ANDROID_H_
#define FIREBASE_UMP_SRC_ANDROID_CONSENT_INFO_INTERNAL_ANDROID_H_

#include <jni.h>
#include <atomic>
#include <vector>

#include "app/src/embedded_file.h"
#include "firebase/internal/mutex.h"
#include "ump/src/common/consent_info_internal.h"

namespace firebase {
namespace ump {
namespace internal {

extern ::firebase::Mutex g_cached_ump_embedded_files_mutex;
extern std::vector<::firebase::internal::EmbeddedFile>*
    g_cached_ump_embedded_files;

class ConsentInfoInternalAndroid : public ConsentInfoInternal {
 public:
  ConsentInfoInternalAndroid(JNIEnv* env, jobject activity);
  ~ConsentInfoInternalAndroid() override;

  void SetActivity(JNIEnv* jni_env, jobject activity) override;
  ConsentStatus GetConsentStatus() override;
  ConsentFormStatus GetConsentFormStatus() override;
  Future<void> RequestConsentInfoUpdate(
      const ConsentRequestParameters& params) override;
  Future<void> LoadConsentForm() override;
  Future<void> ShowConsentForm(FormParent parent) override;
  Future<void> LoadAndShowConsentFormIfRequired(FormParent parent) override;
  PrivacyOptionsRequirementStatus GetPrivacyOptionsRequirementStatus() override;
  Future<void> ShowPrivacyOptionsForm(FormParent parent) override;
  bool CanRequestAds() override;
  void Reset() override;

  bool valid() const { return helper_ != nullptr; }
  JNIEnv* GetJNIEnv();

 private:
  struct EnumCache {
    jint consentstatus_unknown = 0;
    jint consentstatus_required = 0;
    jint consentstatus_not_required = 0;
    jint consentstatus_obtained = 0;
    jint formerror_success = 0;
    jint formerror_internal = 0;
    jint formerror_network = 0;
    jint formerror_invalid_operation = 0;
    jint formerror_timeout = 0;
    jint debug_geography_disabled = 0;
    jint debug_geography_eea = 0;
    jint debug_geography_not_eea = 0;
    jint debug_geography_regulated_us_state = 3;
    jint debug_geography_other = 4;
    jint privacy_options_requirement_unknown = 0;
    jint privacy_options_requirement_required = 0;
    jint privacy_options_requirement_not_required = 0;
    jint function_request_consent_info_update = 0;
    jint function_load_consent_form = 1;
    jint function_show_consent_form = 2;
    jint function_load_and_show_consent_form_if_required = 3;
    jint function_show_privacy_options_form = 4;
    jint function_count = 5;
  };

  static void JNI_ConsentInfoHelper_completeFuture(
      JNIEnv* env, jclass clazz, jint future_fn,
      jlong consent_info_internal_ptr, jlong future_handle, jint error_code,
      jobject error_message_obj);

  void CompleteFutureFromJniCallback(JNIEnv* env, jint future_fn,
                                     FutureHandleId handle_id, int error_code,
                                     const char* error_message);
  bool CacheEnumValues(JNIEnv* env);
  ConsentStatus CppConsentStatusFromAndroid(jint status) const;
  PrivacyOptionsRequirementStatus CppPrivacyOptionsRequirementStatusFromAndroid(
      jint status) const;
  jint AndroidDebugGeographyFromCppDebugGeography(
      ConsentDebugGeography geo) const;
  ConsentRequestError CppConsentRequestErrorFromAndroidFormError(
      jint error, const char* message = nullptr) const;
  ConsentFormError CppConsentFormErrorFromAndroidFormError(
      jint error, const char* message = nullptr) const;
  Future<void> RunFormMethod(ConsentInfoFn fn, int method_id,
                             FormParent parent);

  const EnumCache& enums() const { return enums_; }

  static ConsentInfoInternalAndroid* s_instance;
  static firebase::Mutex s_instance_mutex;

  EnumCache enums_;
  JavaVM* java_vm_ = nullptr;
  jobject helper_ = nullptr;
  jmethodID set_activity_method_id_ = nullptr;
  std::atomic<bool> has_requested_consent_info_update_{false};
};

}  // namespace internal
}  // namespace ump
}  // namespace firebase

#endif  // FIREBASE_UMP_SRC_ANDROID_CONSENT_INFO_INTERNAL_ANDROID_H_
