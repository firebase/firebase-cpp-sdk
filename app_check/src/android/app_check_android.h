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

#ifndef FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_
#define FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_

#include <vector>

#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_android.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

class AppCheckInternal {
 public:
  explicit AppCheckInternal(::firebase::App* app);

  ~AppCheckInternal();

  App* app() const;

  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);

  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);

  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);

  Future<AppCheckToken> GetAppCheckTokenLastResult();

  void AddAppCheckListener(AppCheckListener* listener);

  void RemoveAppCheckListener(AppCheckListener* listener);

  void NotifyTokenChanged(AppCheckToken token);

  FutureManager& future_manager() { return future_manager_; }

  ReferenceCountedFutureImpl* future();

 private:
  ::firebase::App* app_;

  // A Java FirebaseAppCheck instance
  jobject app_check_impl_;

  // A Java AppCheckListener instance
  jobject j_app_check_listener_;

  // A collection of C++ AppCheckListeners
  std::vector<AppCheckListener*> listeners_;

  // Lock object for accessing listeners_.
  Mutex listeners_mutex_;

  FutureManager future_manager_;
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_
