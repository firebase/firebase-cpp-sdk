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

#include <string>

#include "firebase/app.h"
#include "firebase/functions/callable_reference.h"
#include "firebase/functions/callable_result.h"
#include "firebase/functions/common.h"
#include "firebase/internal/common.h"

namespace firebase {

/// @brief Namespace that encompasses all Cloud Functions for Firebase API
/// functionality.
namespace functions {

/// @cond FIREBASE_APP_INTERNAL
namespace internal {
class FunctionsInternal;
}  // namespace internal
/// @endcond

class FunctionsReference;

#ifndef SWIG
/// @brief Entry point for the Firebase C++ SDK for Cloud Functions.
///
/// To use the SDK, call firebase::functions::Functions::GetInstance() to obtain
/// an instance of Functions, then use GetHttpsCallable() to obtain references
/// to callable triggers. From there you can invoke them with
/// HttpsCallableReference::Call().
#endif  // SWIG
class Functions {
 public:
  /// @brief Destructor. You may delete an instance of Functions when you are
  /// finished using it, to shut down the Functions library.
  ~Functions();

  Functions(const Functions&) = delete;
  Functions& operator=(const Functions&) = delete;

  /// @brief Get the default Functions instance for the default Firebase App
  /// in the default region ("us-central1").
  ///
  /// @return The Functions instance, or nullptr if the default App does not
  /// exist.
  static Functions* GetInstance() {
    App* default_app = App::GetInstance();
    return default_app ? GetInstance(default_app) : nullptr;
  }

  /// @brief Get an instance of Functions corresponding to the given App.
  ///
  /// Cloud Functions uses firebase::App to communicate with Firebase Auth to
  /// authenticate users to the Functions server backend.
  ///
  /// If you call GetInstance() multiple times with the same App, you will get
  /// the same instance of Functions.
  ///
  /// @param[in] app Your instance of firebase::App.
  /// @param[out] init_result_out Optional: If provided, write the init result
  /// here. Will be set to kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if Google Play services is
  /// not available on the current device.
  ///
  /// @returns An instance of Functions corresponding to the given App.
  static Functions* GetInstance(::firebase::App* app,
                                InitResult* init_result_out = nullptr);

  /// @brief Get an instance of Functions corresponding to the given App, with
  /// the given region or custom domain.
  ///
  /// Cloud Functions uses firebase::App to communicate with Firebase Auth to
  /// authenticate users to the Functions server backend.
  ///
  /// If you call GetInstance() multiple times with the same App and region, you
  /// will get the same instance of Functions.
  ///
  /// @param[in] app Your instance of firebase::App.
  /// @param[in] region The region (e.g. "us-central1") or custom domain URL.
  /// @param[out] init_result_out Optional: If provided, write the init result
  /// here. Will be set to kInitResultSuccess if initialization succeeded, or
  /// kInitResultFailedMissingDependency on Android if Google Play services is
  /// not available on the current device.
  ///
  /// @returns An instance of Functions corresponding to the given App.
  static Functions* GetInstance(::firebase::App* app, const char* region,
                                InitResult* init_result_out = nullptr);

  /// @brief Get the firebase::App that this Functions was created with.
  ///
  /// @returns The firebase::App this Functions was created with.
  ::firebase::App* app();
  ::firebase::App* app() const;

  /// @brief Get the region or custom domain that this Functions was created
  /// with.
  ///
  /// @returns The region or custom domain string.
  const char* region() const;

  /// @brief Get a FunctionsReference for the specified path.
  ///
  /// @param[in] name The name of the callable function.
  /// @return A HttpsCallableReference targeting the given function name.
  HttpsCallableReference GetHttpsCallable(const char* name) const;

  /// @brief Get a FunctionsReference for the specified path with the given
  /// options.
  ///
  /// @param[in] name The name of the callable function.
  /// @param[in] options The options for configuring the HttpsCallableReference.
  /// @return A HttpsCallableReference targeting the given function name.
  HttpsCallableReference GetHttpsCallable(
      const char* name, const HttpsCallableOptions& options) const;

  /// @brief Get a FunctionsReference for the specified path and a given option.
  ///
  /// @param[in] name The name of the callable function.
  /// @param[in] options The options for configuring the HttpsCallableReference.
  /// @return A HttpsCallableReference targeting the given function name.
  HttpsCallableReference GetHttpsCallableWithOptions(
      const char* name, const HttpsCallableOptions& options) const;

  /// @brief Get a FunctionsReference for the specified URL.
  ///
  /// @param[in] url The URL of the callable trigger.
  /// @return A HttpsCallableReference targeting the given URL.
  HttpsCallableReference GetHttpsCallableFromURL(const char* url) const;

  /// @brief Get a FunctionsReference for the specified URL with the given
  /// options.
  ///
  /// @param[in] url The URL of the callable trigger.
  /// @param[in] options The options for configuring the HttpsCallableReference.
  /// @return A HttpsCallableReference targeting the given URL.
  HttpsCallableReference GetHttpsCallableFromURL(
      const char* url, const HttpsCallableOptions& options) const;

  /// @brief Get a FunctionsReference for the specified URL and a given option.
  ///
  /// @param[in] url The URL of the callable trigger.
  /// @param[in] options The options for configuring the HttpsCallableReference.
  /// @return A HttpsCallableReference targeting the given URL.
  HttpsCallableReference GetHttpsCallableFromURLWithOptions(
      const char* url, const HttpsCallableOptions& options) const;

  /// @brief Modifies this Functions instance to communicate with the Cloud
  /// Functions emulator.
  ///
  /// @param[in] origin The origin of the local emulator, such as
  /// "localhost:5005" or "http://localhost:5005".
  void UseFunctionsEmulator(const char* origin);

  /// @brief Modifies this Functions instance to communicate with the Cloud
  /// Functions emulator at the given host and port.
  ///
  /// @param[in] host The host of the local emulator, such as "localhost".
  /// @param[in] port The port of the local emulator, such as 5001.
  void UseEmulator(const char* host, int port);

 private:
  /// @cond FIREBASE_APP_INTERNAL
  Functions(::firebase::App* app, const char* region);

  // Delete the internal_ data.
  void DeleteInternal();

  internal::FunctionsInternal* internal_;
  /// @endcond
};

}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_H_
