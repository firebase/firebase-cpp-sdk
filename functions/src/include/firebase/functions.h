// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_H_
#define FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_H_

#include <memory>
#include <string>

#include "firebase/app.h"
#include "firebase/functions/callable_reference.h"
#include "firebase/functions/callable_result.h"
#include "firebase/functions/common.h"

namespace firebase {

/// Namespace for the Firebase C++ SDK for Cloud Functions.
namespace functions {

/// @cond FIREBASE_APP_INTERNAL
namespace internal {
class FunctionsInternal;
class HttpsCallableReferenceInternal;
}  // namespace internal
/// @endcond

class FunctionsReference;

#ifndef SWIG
/// @brief Entry point for the Firebase C++ SDK for Cloud Functions.
///
/// To use the SDK, call firebase::functions::Functions::GetInstance() to
/// obtain an instance of Functions, then use GetHttpsCallable() to obtain
/// references to callable functions. From there you can call them with
/// CallableReference::Call().
///
/// Functions is a lightweight copyable and movable value handle sharing
/// ownership of the underlying per-App Functions service.
#endif  // SWIG
class Functions {
 public:
  /// @brief Default constructor. Creates an invalid Functions handle.
  Functions() = default;

  /// @brief Copy constructor.
  Functions(const Functions& other) = default;

  /// @brief Copy assignment operator.
  Functions& operator=(const Functions& other) = default;

  /// @brief Move constructor.
  Functions(Functions&& other) noexcept = default;

  /// @brief Move assignment operator.
  Functions& operator=(Functions&& other) noexcept = default;

  /// @brief Destructor.
  ~Functions() = default;

  /// @brief Returns true if this Functions handle is valid.
  bool is_valid() const noexcept { return internal_ != nullptr; }

  /// @brief Returns true if this Functions handle is valid.
  explicit operator bool() const noexcept { return is_valid(); }

  /// @brief Equality comparison (checks whether two handles refer to the same
  /// underlying Functions instance).
  bool operator==(const Functions& other) const noexcept {
    return internal_ == other.internal_;
  }

  /// @brief Inequality comparison.
  bool operator!=(const Functions& other) const noexcept {
    return internal_ != other.internal_;
  }

  /// @brief Get an instance of Functions corresponding to the given App.
  ///
  /// Cloud Functions uses firebase::App to communicate with Firebase
  /// Authentication to authenticate users to the server backend.
  ///
  /// @param[in] app An instance of firebase::App. Cloud Functions will use
  /// this to communicate with Firebase Authentication.
  /// @param[out] init_result_out Optional: If provided, write the init result
  /// here. Will be set to kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if Google Play services is
  /// not available on the current device.
  ///
  /// @returns A Functions handle corresponding to the given App, or an invalid
  /// handle if initialization failed.
  static Functions GetInstance(const ::firebase::App& app,
                               InitResult* init_result_out = nullptr);

  /// @brief Get an instance of Functions corresponding to the given App.
  ///
  /// Cloud Functions uses firebase::App to communicate with Firebase
  /// Authentication to authenticate users to the server backend.
  ///
  /// @param[in] app An instance of firebase::App. Cloud Functions will use
  /// this to communicate with Firebase Authentication.
  /// @param[in] region The region to call functions in.
  /// @param[out] init_result_out Optional: If provided, write the init result
  /// here. Will be set to kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if Google Play services is
  /// not available on the current device.
  ///
  /// @returns A Functions handle corresponding to the given App, or an invalid
  /// handle if initialization failed.
  static Functions GetInstance(const ::firebase::App& app, const char* region,
                               InitResult* init_result_out = nullptr);

  /// @brief Get the firebase::App that this Functions was created with.
  ///
  /// @returns The firebase::App this Functions was created with, or an invalid
  /// App if this handle is invalid.
  ::firebase::App app() const;

  /// @brief Get a FunctionsReference for the specified path.
  HttpsCallableReference GetHttpsCallable(const char* name) const;

  /// @brief Get a FunctionsReference for the specified path with the given
  /// options.
  HttpsCallableReference GetHttpsCallable(
      const char* name, const HttpsCallableOptions& options) const;

  /// @brief Get a FunctionsReference for the specified URL.
  HttpsCallableReference GetHttpsCallableFromURL(const char* url) const;

  /// @brief Get a FunctionsReference for the specified URL with the given
  /// options.
  HttpsCallableReference GetHttpsCallableFromURL(
      const char* url, const HttpsCallableOptions& options) const;

  /// @brief Sets an origin for a Cloud Functions emulator to use.
  void UseFunctionsEmulator(const char* origin);

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class internal::HttpsCallableReferenceInternal;

  explicit Functions(std::shared_ptr<internal::FunctionsInternal> internal);

  std::shared_ptr<internal::FunctionsInternal> internal_;
  /// @endcond
};

}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_H_
