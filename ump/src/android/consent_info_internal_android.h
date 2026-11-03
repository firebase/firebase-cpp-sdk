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

#ifndef FIREBASE_UMP_SRC_ANDROID_CONSENT_INFO_INTERNAL_ANDROID_H_
#define FIREBASE_UMP_SRC_ANDROID_CONSENT_INFO_INTERNAL_ANDROID_H_

#include <jni.h>

#include <atomic>
#include <vector>

#include "app/src/util_android.h"
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

  bool valid() const { return (helper_ != nullptr); }

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

  // JNI native method callback for ConsentInfoHelper.completeFuture.
  // Calls CompleteFutureFromJniCallback() below.
  static void JNI_ConsentInfoHelper_completeFuture(
      JNIEnv* env, jclass clazz, jint future_fn,
      jlong consent_info_internal_ptr, jlong future_handle, jint error_code,
      jobject error_message_obj);

  // Complete the given Future when called from JNI.
  void CompleteFutureFromJniCallback(JNIEnv* env, jint future_fn,
                                     FutureHandleId handle_id, int error_code,
                                     const char* error_message);

  // Cache Java enum field values in the struct below. Returns false if a JNI
  // exception occurred.
  bool CacheEnumValues(JNIEnv* env);

  // Enum conversion methods.
  ConsentStatus CppConsentStatusFromAndroid(jint status) const;
  PrivacyOptionsRequirementStatus CppPrivacyOptionsRequirementStatusFromAndroid(
      jint status) const;
  jint AndroidDebugGeographyFromCppDebugGeography(
      ConsentDebugGeography geo) const;
  ConsentRequestError CppConsentRequestErrorFromAndroidFormError(
      jint error, const char* message = nullptr) const;
  ConsentFormError CppConsentFormErrorFromAndroidFormError(
      jint error, const char* message = nullptr) const;

  const EnumCache& enums() const { return enums_; }

  static ConsentInfoInternalAndroid* s_instance;
  static firebase::Mutex s_instance_mutex;

  EnumCache enums_;

  JavaVM* java_vm_;
  jobject helper_;
  jmethodID set_activity_method_id_;

  // Needed for GetConsentFormStatus to return Unknown until an update succeeds.
  std::atomic<bool> has_requested_consent_info_update_;
};

}  // namespace internal
}  // namespace ump
}  // namespace firebase

#endif  // FIREBASE_UMP_SRC_ANDROID_CONSENT_INFO_INTERNAL_ANDROID_H_
