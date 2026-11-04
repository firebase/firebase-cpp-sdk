// Copyright 2020 Google LLC
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

#ifndef FIREBASE_INSTALLATIONS_SRC_INCLUDE_FIREBASE_INSTALLATIONS_H_
#define FIREBASE_INSTALLATIONS_SRC_INCLUDE_FIREBASE_INSTALLATIONS_H_

#include <cstdint>
#include <string>

#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

#if !defined(DOXYGEN) && !defined(SWIG)
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(installations)
#endif  // !defined(DOXYGEN) && !defined(SWIG)

namespace firebase {
namespace installations {

enum Error {
  kErrorNone = 0,
  kInstallationsErrorNone = kErrorNone,
  kErrorUnknown = 1,
  kErrorFailure = kErrorUnknown,
  kInstallationsErrorFailure = kErrorUnknown,
  kErrorNoAccess = 2,
  kErrorInvalidConfiguration = 3,
  kErrorUnableToLoad = 4,
};

#if !defined(SWIG)
const char* GetErrorMessage(Error error) noexcept;

struct InstallationTokenResult {
  std::string token;
  std::int64_t expiration_timestamp = 0;

  bool operator==(const InstallationTokenResult& other) const noexcept {
    return token == other.token &&
           expiration_timestamp == other.expiration_timestamp;
  }
  bool operator!=(const InstallationTokenResult& other) const noexcept {
    return !(*this == other);
  }
};
#endif  // !defined(SWIG)

#if !defined(DOXYGEN) && !defined(SWIG)
namespace internal {
class InstallationsInternal;
}  // namespace internal
#endif  // !defined(DOXYGEN) && !defined(SWIG)

class Installations {
 public:
  ~Installations();

  Installations(const Installations&) = delete;
  Installations& operator=(const Installations&) = delete;

  App* app() const noexcept { return app_; }

#if !defined(SWIG)
  bool is_valid() const noexcept {
    return app_ != nullptr && installations_internal_ != nullptr;
  }
  static Installations* GetInstance();
#endif  // !defined(SWIG)

  static Installations* GetInstance(App* app);

#if !defined(SWIG)
  static Installations* GetInstance(App* app, InitResult* init_result_out);
#endif  // !defined(SWIG)

  Future<std::string> GetId();
  Future<std::string> GetIdLastResult() const;

  Future<void> Delete();
  Future<void> DeleteLastResult() const;

  Future<std::string> GetToken(bool force_refresh);
#if !defined(SWIG)
  Future<std::string> GetToken() { return GetToken(false); }
#endif  // !defined(SWIG)
  Future<std::string> GetTokenLastResult() const;

#if !defined(SWIG)
  Future<InstallationTokenResult> GetTokenResult(bool force_refresh = false);
  Future<InstallationTokenResult> GetTokenResultLastResult() const;
#endif  // !defined(SWIG)

 private:
  explicit Installations(App* app);

  static Installations* FindInstallationsLocked(App* app);
  static Installations* FindInstallations(App* app);
  bool InitInternal();
  void DeleteInternal();
  void DeleteInternalLocked();

  App* app_ = nullptr;
  internal::InstallationsInternal* installations_internal_ = nullptr;
};

}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_INSTALLATIONS_SRC_INCLUDE_FIREBASE_INSTALLATIONS_H_
