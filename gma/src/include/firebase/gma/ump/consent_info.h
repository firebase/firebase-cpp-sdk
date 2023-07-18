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

#ifndef FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_UMP_CONSENT_INFO_H_
#define FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_UMP_CONSENT_INFO_H_

#include "firebase/gma/ump/types.h"
#include "firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include <jni.h>
#endif  // FIREBASE_PLATFORM_ANDROID

#include <vector>

#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace gma {
/// @brief API for User Messaging Platform.
///
/// The User Messaging Platform (UMP) SDK is Google’s option to handle user
/// privacy and consent in mobile apps.
namespace ump {

namespace internal {
// Forward declaration for platform-specific data, implemented in each library.
class ConsentInfoInternal;
}  // namespace internal

class ConsentInfo {
 public:
  /// Initializes the User Messaging Platform Consent API.
  ///
  /// @param[in] app Any Firebase App instance.
  ///
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

#if FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  /// Initializes the User Messaging Platform Consent API without Firebase for
  /// Android.
  ///
  /// The arguments to @ref Initialize are platform-specific so the caller must
  /// do something like this:
  /// @code
  /// #if defined(__ANDROID__)
  /// consent_info = firebase::gma::ump::ConsentInfo::GetInstance(jni_env,
  /// activity); #else consent_info = firebase::gma::ump::GetInstance(); #endif
  /// @endcode
  ///
  /// @param[in] jni_env JNIEnv pointer.
  /// @param[in] activity Activity used to start the application.
  /// @param[out] init_result_out Optional: If provided, write the basic init
  /// result here. kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if there are Android
  /// dependencies missing.
  ///
  /// @return A pointer to the ConsentInfo instance if UMP was successfully
  /// initialized, nullptr otherwise. Each call to GetInstance() will return the
  /// same pointer; when you are finished using the SDK, you can delete the
  /// pointer and the UMP SDK will shut down.
  static ConsentInfo* GetInstance(JNIEnv* jni_env, jobject activity,
                                  InitResult* init_result_out = nullptr);
#endif  // FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)

#if !FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  /// Initializes Google Mobile Ads (GMA) without Firebase for iOS.
  ///
  /// @param[out] init_result_out Optional: If provided, write the basic init
  /// result here. kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency if a dependency is missing. On iOS,
  /// this will always return kInitResultSuccess, as missing dependencies would
  /// have caused a linker error at build time.
  ///
  /// @return A pointer to the ConsentInfo instance. Each call to GetInstance()
  /// will return the same pointer; when you are finished using the SDK, you can
  /// delete the pointer, and the UMP SDK will shut down.
  static ConsentInfo* GetInstance(InitResult* init_result_out = nullptr);
#endif  // !defined(__ANDROID__) || defined(DOXYGEN)

  /// Requests consent information update. Must be called before loading a
  /// consent form.
  Future<ConsentStatus> RequestConsentStatus(
      const ConsentRequestParameters& params);

  /// The user’s consent status. This value is cached between app sessions and
  /// can be read before calling RequestConsentStatus().
  ConsentStatus GetConsentStatus();

  /// Consent form status. This value defaults to kConsentFormStatusUnknown and
  /// requires a call to RequestConsentStatus() to update.
  ConsentFormStatus GetConsentFormStatus();

  /// Loads a consent form.
  Future<ConsentFormStatus> LoadConsentForm();

  /// Presents the full screen consent form using the given FormParent, which is
  /// defined as an Activity on Android and a UIViewController on iOS. The form
  /// will be dismissed and the Future will be completed after the user selects
  /// an option. GetConsentStatus() is updated prior to the Future being
  /// completed.
  Future<ConsentStatus> ShowConsentForm(FormParent parent);

  /// Clears all consent state from persistent storage. This can be used in
  /// development to simulate a new installation.
  void Reset();

 private:
  // An internal, platform-specific implementation object that this class uses
  // to interact with the User Messaging Platform SDKs for iOS and Android.
  internal::ConsentInfoInternal* internal_;
};

}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_UMP_CONSENT_INFO_H_
