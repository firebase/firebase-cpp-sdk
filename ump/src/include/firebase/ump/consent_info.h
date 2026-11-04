// Copyright 2023 Google LLC

#ifndef FIREBASE_UMP_SRC_INCLUDE_FIREBASE_UMP_CONSENT_INFO_H_
#define FIREBASE_UMP_SRC_INCLUDE_FIREBASE_UMP_CONSENT_INFO_H_

#if !defined(SWIG)
#include <type_traits>
#endif  // !defined(SWIG)

#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/platform.h"
#include "firebase/ump/types.h"

#if FIREBASE_PLATFORM_ANDROID
#include <jni.h>
#endif  // FIREBASE_PLATFORM_ANDROID

namespace firebase {
namespace ump {

namespace internal {
class ConsentInfoInternal;
}  // namespace internal

class ConsentInfo {
 public:
  ~ConsentInfo();

  ConsentInfo(const ConsentInfo&) = delete;
  ConsentInfo& operator=(const ConsentInfo&) = delete;

  static ConsentInfo* GetInstance(const ::firebase::App& app,
                                  InitResult* init_result_out = nullptr);
  static ConsentInfo* GetInstance(::firebase::App* app,
                                  InitResult* init_result_out);

#if !defined(SWIG)
  template <typename AppPtr,
            typename std::enable_if<
                std::is_same<AppPtr, ::firebase::App*>::value, int>::type = 0>
  static ConsentInfo* GetInstance(AppPtr app) {
    return GetInstance(app, nullptr);
  }
#endif  // !defined(SWIG)

#if FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  static ConsentInfo* GetInstance(JNIEnv* jni_env, jobject activity,
                                  InitResult* init_result_out = nullptr);
  void SetActivity(JNIEnv* jni_env, jobject activity);
#endif  // FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)

  static ConsentInfo* GetInstance(InitResult* init_result_out = nullptr);

  ::firebase::App* app() const { return app_; }

  ConsentStatus GetConsentStatus();
  Future<void> RequestConsentInfoUpdate(
      const ConsentRequestParameters& params = ConsentRequestParameters());
  Future<void> RequestConsentInfoUpdateLastResult();

  ConsentFormStatus GetConsentFormStatus();
  Future<void> LoadConsentForm();
  Future<void> LoadConsentFormLastResult();

  Future<void> ShowConsentForm(FormParent parent = nullptr);
  Future<void> ShowConsentFormLastResult();

  Future<void> LoadAndShowConsentFormIfRequired(FormParent parent = nullptr);
  Future<void> LoadAndShowConsentFormIfRequiredLastResult();

  PrivacyOptionsRequirementStatus GetPrivacyOptionsRequirementStatus();
  Future<void> ShowPrivacyOptionsForm(FormParent parent = nullptr);
  Future<void> ShowPrivacyOptionsFormLastResult();

  bool CanRequestAds();
  void Reset();

 private:
  ConsentInfo();
#if FIREBASE_PLATFORM_ANDROID
  InitResult Initialize(JNIEnv* jni_env, jobject activity);
#else
  InitResult Initialize();
#endif  // FIREBASE_PLATFORM_ANDROID
  void BindToApp(::firebase::App* app);
  void UnbindFromApp();
  void OnAppDestroyed();

  static ConsentInfo* s_instance_;
  ::firebase::App* app_;
  internal::ConsentInfoInternal* internal_;
};

}  // namespace ump
}  // namespace firebase

#endif  // FIREBASE_UMP_SRC_INCLUDE_FIREBASE_UMP_CONSENT_INFO_H_
