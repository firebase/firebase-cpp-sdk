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
/// @brief API for User Messaging Platform.
///
/// The User Messaging Platform (UMP) SDK is Google’s option to handle user
/// privacy and consent in mobile apps.
namespace ump {

namespace internal {
// Forward declaration for platform-specific data, implemented in each library.
class ConsentInfoInternal;
}  // namespace internal

/// @brief Consent Information class for the User Messaging Platform SDK.
///
/// The User Messaging Platform (UMP) SDK is Google’s option to handle user
/// privacy and consent in mobile apps.
///
/// This class contains all of the methods necessary for obtaining
/// consent from the user.
class ConsentInfo {
 public:
  /// Shut down the User Messaging Platform Consent SDK.
  ~ConsentInfo();

  ConsentInfo(const ConsentInfo&) = delete;
  ConsentInfo& operator=(const ConsentInfo&) = delete;

  /// Initializes the User Messaging Platform Consent SDK.
  ///
  /// @param[in] app Any Firebase App instance.
  /// @param[out] init_result_out Optional: If provided, write the basic init
  /// result here. kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if there are Android
  /// dependencies missing.
  ///
  /// @return A pointer to the ConsentInfo instance if UMP was successfully
  /// initialized, nullptr otherwise. Each call to GetInstance() will return the
  /// same pointer; when you are finished using the SDK, you can delete the
  /// pointer and the UMP SDK will shut down.
  static ConsentInfo* GetInstance(const ::firebase::App& app,
                                  InitResult* init_result_out = nullptr);

  /// Initializes the User Messaging Platform Consent SDK from a Firebase App
  /// pointer.
  static ConsentInfo* GetInstance(::firebase::App* app,
                                  InitResult* init_result_out);

#if !defined(SWIG)
  /// Single-argument pointer overload disambiguated via SFINAE so that
  /// `ConsentInfo::GetInstance(nullptr)` resolves unambiguously to
  /// `GetInstance(InitResult* = nullptr)`.
  template <typename AppPtr,
            typename std::enable_if<
                std::is_same<AppPtr, ::firebase::App*>::value, int>::type = 0>
  static ConsentInfo* GetInstance(AppPtr app) {
    return GetInstance(app, nullptr);
  }
#endif  // !defined(SWIG)

#if FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  /// Initializes the User Messaging Platform Consent SDK without Firebase for
  /// Android.
  ///
  /// @param[in] jni_env JNIEnv pointer.
  /// @param[in] activity Activity used to start the application.
  /// @param[out] init_result_out Optional: If provided, write the basic init
  /// result here. kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if there are Android
  /// dependencies missing.
  ///
  /// @return A pointer to the ConsentInfo instance if UMP was successfully
  /// initialized, nullptr otherwise.
  static ConsentInfo* GetInstance(JNIEnv* jni_env, jobject activity,
                                  InitResult* init_result_out = nullptr);

  /// Updates the active Android Activity reference after a configuration change
  /// or Activity recreation.
  void SetActivity(JNIEnv* jni_env, jobject activity);
#endif  // FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)

  /// Obtains the existing `ConsentInfo` instance, or initializes UMP using the
  /// default `firebase::App` (on Android) or platform SDK (on iOS/Desktop).
  static ConsentInfo* GetInstance(InitResult* init_result_out = nullptr);

  /// Returns the `firebase::App` this `ConsentInfo` was initialized with, or
  /// `nullptr` if initialized without Firebase.
  ::firebase::App* app() const { return app_; }

  /// The user’s consent status. This value defaults to kConsentStatusUnknown
  /// until RequestConsentInfoUpdate() is called, and defaults to the previous
  /// session’s value until RequestConsentInfoUpdate() completes.
  ConsentStatus GetConsentStatus();

  /// Requests consent information update. Must be called in every app session
  /// before checking the user’s consent status or loading a consent form.
  Future<void> RequestConsentInfoUpdate(
      const ConsentRequestParameters& params = ConsentRequestParameters());

  /// Get the Future from the most recent call to RequestConsentInfoUpdate().
  Future<void> RequestConsentInfoUpdateLastResult();

  /// Consent form status. This value defaults to kConsentFormStatusUnknown and
  /// requires a call to RequestConsentInfoUpdate() to update.
  ConsentFormStatus GetConsentFormStatus();

  /// Loads a consent form. Returns an error if the consent form is unavailable
  /// or cannot be loaded.
  Future<void> LoadConsentForm();

  /// Get the Future from the most recent call to LoadConsentForm().
  Future<void> LoadConsentFormLastResult();

  /// Presents the full screen consent form using the given FormParent, which is
  /// defined as an Activity on Android and a UIViewController on iOS.
  Future<void> ShowConsentForm(FormParent parent = nullptr);

  /// Get the Future from the most recent call to ShowConsentForm().
  Future<void> ShowConsentFormLastResult();

  /// Loads a consent form and immediately presents it using the given
  /// FormParent, if ConsentStatus is kConsentStatusRequired.
  Future<void> LoadAndShowConsentFormIfRequired(FormParent parent = nullptr);

  /// Get the Future from the most recent call to
  /// LoadAndShowConsentFormIfRequired().
  Future<void> LoadAndShowConsentFormIfRequiredLastResult();

  /// Check whether the privacy options form needs to be displayed.
  /// This is updated by RequestConsentInfoUpdate().
  PrivacyOptionsRequirementStatus GetPrivacyOptionsRequirementStatus();

  /// If GetPrivacyOptionsRequirementStatus() is
  /// kPrivacyOptionsRequirementStatusRequired, presents a privacy options form
  /// from the provided FormParent.
  Future<void> ShowPrivacyOptionsForm(FormParent parent = nullptr);

  /// Get the Future from the most recent call to ShowPrivacyOptionsForm().
  Future<void> ShowPrivacyOptionsFormLastResult();

  /// Indicates whether the app has completed the necessary steps for gathering
  /// updated user consent.
  bool CanRequestAds();

  /// Clears all consent state from persistent storage. This can be used in
  /// development to simulate a new installation.
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

  // An internal, platform-specific implementation object that this class uses
  // to interact with the User Messaging Platform SDKs for iOS and Android.
  internal::ConsentInfoInternal* internal_;
};

}  // namespace ump
}  // namespace firebase

#endif  // FIREBASE_UMP_SRC_INCLUDE_FIREBASE_UMP_CONSENT_INFO_H_
