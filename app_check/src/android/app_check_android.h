// Copyright 2022 Google LLC

#ifndef FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_
#define FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_

#include <string>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app_check/src/common/common.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase::app_check::internal {

class AppCheckInternal {
 public:
  explicit AppCheckInternal(::firebase::App* app);
  ~AppCheckInternal();

  App* app() const { return app_; }
  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);
  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);
  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);
  Future<AppCheckToken> GetAppCheckTokenLastResult();
  Future<AppCheckToken> GetLimitedUseAppCheckToken();
  Future<AppCheckToken> GetLimitedUseAppCheckTokenLastResult();
  void AddAppCheckListener(AppCheckListener* listener);
  void RemoveAppCheckListener(AppCheckListener* listener);
  void NotifyTokenChanged(AppCheckToken token);
  ReferenceCountedFutureImpl* future() { return &future_impl_; }
  ReferenceCountedFutureImpl future_impl_;

 private:
  Future<AppCheckToken> DispatchTokenTask(AppCheckFn fn, bool force_refresh);

  ::firebase::App* app_;
  jobject app_check_impl_;
  jobject j_app_check_listener_;
  std::vector<AppCheckListener*> listeners_;
  Mutex listeners_mutex_;
  std::string jni_task_id_;
};

}  // namespace firebase::app_check::internal

#endif  // FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_
