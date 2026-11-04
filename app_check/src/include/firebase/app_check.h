// Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_
#define FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_

#include <cstdint>
#include <functional>
#include <string>

#include "firebase/app.h"
#include "firebase/future.h"

namespace firebase {
namespace app_check {

enum AppCheckError {
  kAppCheckErrorNone = 0,
  kAppCheckErrorServerUnreachable = 1,
  kAppCheckErrorInvalidConfiguration = 2,
  kAppCheckErrorSystemKeychain = 3,
  kAppCheckErrorUnsupportedProvider = 4,
  kAppCheckErrorUnknown = 5,
  kAppCheckErrorTooManyRequests = 6,
};

struct AppCheckToken {
  std::string token = "";
  int64_t expire_time_millis = 0;

#if !defined(SWIG)
  [[nodiscard]] bool is_valid() const noexcept {
    return !token.empty() && expire_time_millis > 0;
  }
  [[nodiscard]] bool is_valid(int64_t now_millis,
                              int64_t buffer_millis = 300000) const noexcept {
    return !token.empty() && (expire_time_millis - buffer_millis > now_millis);
  }
  friend bool operator==(const AppCheckToken& lhs,
                         const AppCheckToken& rhs) noexcept {
    return lhs.token == rhs.token &&
           lhs.expire_time_millis == rhs.expire_time_millis;
  }
  friend bool operator!=(const AppCheckToken& lhs,
                         const AppCheckToken& rhs) noexcept {
    return !(lhs == rhs);
  }
#endif  // !defined(SWIG)
};

class AppCheckListener {
 public:
  virtual ~AppCheckListener() = 0;
  virtual void OnAppCheckTokenChanged(const AppCheckToken& token) = 0;
};

class AppCheckProvider {
 public:
  virtual ~AppCheckProvider() = 0;
  virtual void GetToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) = 0;
  virtual void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback);
};

class AppCheckProviderFactory {
 public:
  virtual ~AppCheckProviderFactory() = 0;
  virtual AppCheckProvider* CreateProvider(App* app) = 0;
};

namespace internal {
class AppCheckInternal;
}  // namespace internal

class AppCheck {
 public:
  ~AppCheck();

#if !defined(DOXYGEN) && !defined(SWIG)
  AppCheck(const AppCheck&) = delete;
  AppCheck& operator=(const AppCheck&) = delete;
#endif  // !defined(DOXYGEN) && !defined(SWIG)

  static AppCheck* GetInstance(::firebase::App* app);
#if !defined(SWIG)
  static AppCheck* GetInstance(::firebase::App* app,
                               InitResult* init_result_out);
#endif  // !defined(SWIG)

  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);

  ::firebase::App* app();
#if !defined(SWIG)
  const ::firebase::App* app() const;
#endif  // !defined(SWIG)

  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);

  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);
#if !defined(SWIG)
  Future<AppCheckToken> GetAppCheckToken() { return GetAppCheckToken(false); }
#endif  // !defined(SWIG)
  Future<AppCheckToken> GetAppCheckTokenLastResult();

  Future<AppCheckToken> GetLimitedUseAppCheckToken();
  Future<AppCheckToken> GetLimitedUseAppCheckTokenLastResult();

  void AddAppCheckListener(AppCheckListener* listener);
  void RemoveAppCheckListener(AppCheckListener* listener);

 private:
  explicit AppCheck(::firebase::App* app);
  void DeleteInternal();

  friend class internal::AppCheckInternal;
  internal::AppCheckInternal* internal_{nullptr};
};

}  // namespace app_check
}  // namespace firebase

#include "firebase/app_check/app_attest_provider.h"
#include "firebase/app_check/debug_provider.h"
#include "firebase/app_check/device_check_provider.h"
#include "firebase/app_check/play_integrity_provider.h"

#endif  // FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_
