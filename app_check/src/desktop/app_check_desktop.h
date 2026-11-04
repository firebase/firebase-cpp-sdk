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

#ifndef FIREBASE_APP_CHECK_SRC_DESKTOP_APP_CHECK_DESKTOP_H_
#define FIREBASE_APP_CHECK_SRC_DESKTOP_APP_CHECK_DESKTOP_H_

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase::app_check::internal {

using FunctionRegistryCallback = void (*)(const std::string&, void*);

class FunctionRegistryAppCheckListener : public AppCheckListener {
 public:
  bool AddListener(FunctionRegistryCallback callback, void* context);
  void RemoveListener(FunctionRegistryCallback callback, void* context);
  void OnAppCheckTokenChanged(const AppCheckToken& token) override;

 private:
  using Entry = std::pair<FunctionRegistryCallback, void*>;
  mutable std::mutex mutex_;
  std::vector<Entry> callbacks_;
};

class AppCheckInternal {
 public:
  struct LifetimeState {
    std::mutex mutex;
    std::condition_variable cv;
    AppCheckInternal* owner{nullptr};
    std::vector<std::thread::id> active_threads;
  };

  explicit AppCheckInternal(::firebase::App* app);
  ~AppCheckInternal();

  App* app() const { return app_; }

  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);

  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);
  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);
  Future<AppCheckToken> GetAppCheckTokenLastResult();
  Future<AppCheckToken> GetLimitedUseAppCheckToken();
  Future<AppCheckToken> GetLimitedUseAppCheckTokenLastResult();

  Future<std::string> GetAppCheckTokenStringInternal(bool force_refresh = false);
  Future<std::string> GetLimitedUseAppCheckTokenStringInternal();

  void AddAppCheckListener(AppCheckListener* listener);
  void RemoveAppCheckListener(AppCheckListener* listener);
  ReferenceCountedFutureImpl* future() { return &future_impl_; }
  ReferenceCountedFutureImpl future_impl_;

 private:
  bool HasValidCacheToken() const;
  bool HasValidCacheTokenLocked() const;
  void UpdateCachedToken(AppCheckToken token);
  AppCheckProvider* GetProvider();
  void InitRegistryCalls();
  void CleanupRegistryCalls();

  static bool GetAppCheckTokenAsyncForRegistry(App* app, void* force_refresh_ptr,
                                               void* out_future);
  static bool GetLimitedUseAppCheckTokenAsyncForRegistry(App* app,
                                                         void* /*unused*/,
                                                         void* out_future);
  static bool AddAppCheckListenerForRegistry(App* app, void* callback,
                                             void* context);
  static bool RemoveAppCheckListenerForRegistry(App* app, void* callback,
                                                void* context);

  ::firebase::App* app_{nullptr};
  mutable std::mutex mutex_;
  std::shared_ptr<LifetimeState> lifetime_state_;
  AppCheckProvider* cached_provider_{nullptr};
  AppCheckProviderFactory* cached_provider_factory_{nullptr};
  AppCheckToken cached_token_{};
  std::list<AppCheckListener*> token_listeners_;
  FunctionRegistryAppCheckListener internal_listener_;
  bool is_token_auto_refresh_enabled_{true};
};

}  // namespace firebase::app_check::internal

#endif  // FIREBASE_APP_CHECK_SRC_DESKTOP_APP_CHECK_DESKTOP_H_
