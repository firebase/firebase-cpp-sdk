/*
 * Copyright 2022 Google LLC
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

#ifndef FIREBASE_AUTH_SRC_CREDENTIAL_INTERNAL_H_
#define FIREBASE_AUTH_SRC_CREDENTIAL_INTERNAL_H_

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/include/firebase/auth/credential.h"

#if FIREBASE_PLATFORM_IOS
#include "firebase/auth/client/cpp/src/ios/common_ios.h"
#elif FIREBASE_PLATFORM_ANDROID
#include <jni.h>

#include "firebase/app/client/cpp/src/util_android.h"
#include "firebase/auth/client/cpp/src/android/common_android.h"
#elif FIREBASE_PLATFORM_DESKTOP
#include "firebase/auth/client/cpp/src/desktop/credential_impl.h"
#endif

#if FIREBASE_PLATFORM_IOS
// Used to create a credential by CredentialInternal::Create().
typedef FIRAuthCredential* _Nullable (^CreatePlatformCredentialCallback)();
#endif  // FIREBASE_PLATFORM_IOS

namespace firebase {
namespace auth {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

// Class that has access to the internals of the Credential object.
class CredentialInternal {
 public:
#if FIREBASE_PLATFORM_DESKTOP
  // Get the platform specific implementation pointer from the credential.
  static inline void* GetPlatformCredential(const Credential& credential) {
    return static_cast<CredentialImpl*>(credential.impl_);
  }
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_IOS
  /// Convert from the void* credential implementation pointer into the
  /// reference to the FIRAuthCredential object.
  static inline FIRAuthCredentialPointer* GetPlatformCredential(
      const Credential& credential) {
    return static_cast<FIRAuthCredentialPointer *>(credential.impl_);
  }
#endif  // FIREBASE_PLATFORM_IOS


#if FIREBASE_PLATFORM_ANDROID
  // Return the Java Credential class from our platform-independent
  // representation.
  static inline util::JObjectReference* GetPlatformCredential(
      const Credential& credential) {
    return static_cast<util::JObjectReference*>(credential.impl_);
  }
#endif  // FIREBASE_PLATFORM_ANDROID

#if FIREBASE_PLATFORM_IOS
  // Construct a credential using the return value from the specific block.
  // If the block throws an exception, construct the credential with an error
  // code and message.
  static Credential Create(
      CreatePlatformCredentialCallback create_platform_credential);
#endif  // FIREBASE_PLATFORM_IOS

#if FIREBASE_PLATFORM_ANDROID
  // Construct a credential from the specified object with error code and
  // message.
  static inline Credential Create(JNIEnv *env, jobject platform_credential,
                                  AuthError error_code,
                                  const std::string& error_message) {
    return Credential(
        platform_credential ? new util::JObjectReference(
            util::JObjectReference::FromLocalReference(
                env, platform_credential)) : nullptr,
        error_code, error_message);
  }

  // Construct a credential from the specified object checking for any pending
  // JNI exceptions.
  static inline Credential Create(JNIEnv *env, jobject platform_credential) {
    std::string error_message;
    AuthError error_code = CheckAndClearJniAuthExceptions(env, &error_message);
    if (error_code != kAuthErrorNone) {
      if (platform_credential) {
        env->DeleteLocalRef(platform_credential);
        platform_credential = nullptr;
      }
    }
    return Create(env, platform_credential, error_code, error_message);
  }
#endif  // FIREBASE_PLATFORM_ANDROID

  // Checks if a credential is in an error state and if so completes the
  // specified future with the error and returns true.  If the credential is
  // valid this returns false.
  template <typename T>
  static bool CompleteFutureIfInvalid(
      const Credential& credential,
      ReferenceCountedFutureImpl* _Nonnull futures,
      const SafeFutureHandle<T>& handle) {
    if (credential.error_code_ != kAuthErrorNone) {
      futures->Complete(handle, credential.error_code_,
                        credential.error_message_.c_str());
      return true;
    } else if (credential.impl_ == nullptr) {
      futures->Complete(handle, kAuthErrorInvalidCredential,
                        "Invalid credential");
    }
    return false;
  }
};

#pragma clang diagnostic pop

}  // namespace auth
}  // namespace firebase

#endif FIREBASE_AUTH_SRC_CREDENTIAL_INTERNAL_H_