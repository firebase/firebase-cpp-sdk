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

#ifndef FIREBASE_APP_CHECK_SRC_DESKTOP_DEBUG_PROVIDER_DESKTOP_H_
#define FIREBASE_APP_CHECK_SRC_DESKTOP_DEBUG_PROVIDER_DESKTOP_H_

#include <functional>
#include <map>
#include <mutex>
#include <string>

#include "app/src/scheduler.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

class DebugAppCheckProviderFactoryInternal;

// Reads and parses the local configstore JSON file at `config_path` and
// returns the debug token for `app_id`, or an empty string if not found or
// invalid.
std::string ResolveDebugTokenFromConfig(const std::string& config_path,
                                        const std::string& app_id);

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  DebugAppCheckProvider(
      App* app, const std::string& token,
      DebugAppCheckProviderFactoryInternal* factory = nullptr);
  ~DebugAppCheckProvider() override;

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override;

  void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) override;

  // Resolves the debug token (if not already set) following the priority order:
  // 1. Explicit SetDebugToken
  // 2. Environment variable (FIREBASE_APPCHECK_DEBUG_TOKEN /
  // AppCheckDebugToken)
  // 3. Local configstore (~/.config/configstore/firebase-appcheck.json)
  // 4. Auto-generated UUID
  std::string DetermineDebugToken();

  App* app() const { return app_; }
  DebugAppCheckProviderFactoryInternal* factory() const { return factory_; }

 private:
  void GetTokenInternal(
      bool limited_use,
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback);

  App* app_;
  DebugAppCheckProviderFactoryInternal* factory_;
  scheduler::Scheduler scheduler_;
  std::mutex token_mutex_;
  std::string debug_token_;
  bool logged_token_;
};

class DebugAppCheckProviderFactoryInternal : public AppCheckProviderFactory {
 public:
  DebugAppCheckProviderFactoryInternal();

  virtual ~DebugAppCheckProviderFactoryInternal();

  AppCheckProvider* CreateProvider(App* app) override;

  void SetDebugToken(const std::string& token);
  std::string GetDebugToken();

  void RemoveProviderForApp(App* app);

 private:
  std::mutex mutex_;
  std::map<App*, AppCheckProvider*> provider_map_;

  std::string debug_token_;
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_DESKTOP_DEBUG_PROVIDER_DESKTOP_H_
