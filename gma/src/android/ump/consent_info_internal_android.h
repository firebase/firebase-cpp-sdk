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

#ifndef FIREBASE_GMA_SRC_ANDROID_UMP_CONSENT_INFO_INTERNAL_ANDROID_H_
#define FIREBASE_GMA_SRC_ANDROID_UMP_CONSENT_INFO_INTERNAL_ANDROID_H_

#include <jni.h>

#include "app/src/util_android.h"
#include "firebase/internal/mutex.h"
#include "gma/src/common/ump/consent_info_internal.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

class ConsentInfoInternalAndroid : public ConsentInfoInternal {
 public:
  ConsentInfoInternalAndroid(JNIEnv* env, jobject activity);
  ~ConsentInfoInternalAndroid() override;

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

  bool valid() { return (helper_ != nullptr); }

  JNIEnv* GetJNIEnv();
  jobject activity();

 private:
  struct EnumCache {
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

    jint function_request_consent_info_update;
    jint function_load_consent_form;
    jint function_show_consent_form;
    jint function_load_and_show_consent_form_if_required;
    jint function_show_privacy_options_form;
    jint function_count;
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

  // Cache Java enum field values in the struct below.
  void CacheEnumValues(JNIEnv* env);

  // Enum conversion methods.
  ConsentStatus CppConsentStatusFromAndroid(jint status);
  PrivacyOptionsRequirementStatus CppPrivacyOptionsRequirementStatusFromAndroid(
      jint status);
  jint AndroidDebugGeographyFromCppDebugGeography(ConsentDebugGeography geo);
  ConsentRequestError CppConsentRequestErrorFromAndroidFormError(
      jint error, const char* message = nullptr);
  ConsentFormError CppConsentFormErrorFromAndroidFormError(
      jint error, const char* message = nullptr);

  const EnumCache& enums() { return enums_; }

  static ConsentInfoInternalAndroid* s_instance;
  static firebase::Mutex s_instance_mutex;

  EnumCache enums_;

  JavaVM* java_vm_;
  jobject activity_;
  jobject helper_;

  // Needed for GetConsentFormStatus to return Unknown.
  bool has_requested_consent_info_update_;
};

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_ANDROID_UMP_CONSENT_INFO_INTERNAL_ANDROID_H_
