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
namespace functions {

/// @cond FIREBASE_APP_INTERNAL
namespace internal {
class FunctionsInternal;
}  // namespace internal
/// @endcond

class FunctionsReference;

/// @brief Entry point for the Firebase C++ SDK for Cloud Functions.
class Functions {
 public:
  ~Functions();

  Functions(const Functions&) = delete;
  Functions& operator=(const Functions&) = delete;
  Functions(Functions&&) = delete;
  Functions& operator=(Functions&&) = delete;

  static Functions* GetInstance() {
    App* default_app = App::GetInstance();
    return default_app ? GetInstance(default_app) : nullptr;
  }

  static Functions* GetInstance(::firebase::App* app,
                                InitResult* init_result_out = nullptr);

  static Functions* GetInstance(::firebase::App* app, const char* region,
                                InitResult* init_result_out = nullptr);

  ::firebase::App* app();
  ::firebase::App* app() const;

  const char* region() const;

  HttpsCallableReference GetHttpsCallable(const char* name) const;
  HttpsCallableReference GetHttpsCallable(
      const char* name, const HttpsCallableOptions& options) const;
  HttpsCallableReference GetHttpsCallableWithOptions(
      const char* name, const HttpsCallableOptions& options) const;

  HttpsCallableReference GetHttpsCallableFromURL(const char* url) const;
  HttpsCallableReference GetHttpsCallableFromURL(
      const char* url, const HttpsCallableOptions& options) const;
  HttpsCallableReference GetHttpsCallableFromURLWithOptions(
      const char* url, const HttpsCallableOptions& options) const;

  void UseFunctionsEmulator(const char* origin);
  void UseEmulator(const char* host, int port);

 private:
  /// @cond FIREBASE_APP_INTERNAL
  Functions(::firebase::App* app, const char* region);
  void DeleteInternal();

  internal::FunctionsInternal* internal_;
  /// @endcond
};

}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_H_
