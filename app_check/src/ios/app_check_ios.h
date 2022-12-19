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

#ifndef FIREBASE_APP_CHECK_SRC_IOS_APP_CHECK_IOS_H_
#define FIREBASE_APP_CHECK_SRC_IOS_APP_CHECK_IOS_H_

#include "app/memory/unique_ptr.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/util_ios.h"
#include "app_check/src/include/firebase/app_check.h"

#ifdef __OBJC__
#import "FIRAppCheck.h"
#endif  // __OBJC__

#ifdef __OBJC__
// Interacts with the default notification center.
@interface AppCheckNotificationCenterWrapper : NSObject

- (id)init;

- (void)stopListening;

- (void)addListener:(firebase::app_check::AppCheckListener* _Nonnull)listener;

- (void)removeListener:(firebase::app_check::AppCheckListener* _Nonnull)listener;

- (void)appCheckTokenDidChangeNotification:(NSNotification*)notification;

@end
#endif  // __OBJC__

namespace firebase {
namespace app_check {
namespace internal {

// This defines the class AppCheckNotificationCenterWrapperPointer, which is a
// C++-compatible wrapper around the AppCheckNotificationCenterWrapper Obj-C
// class.
OBJ_C_PTR_WRAPPER(AppCheckNotificationCenterWrapper);

// This defines the class FIRAppCheckPointer, which is a C++-compatible
// wrapper around the FIRAppCheck Obj-C class.
OBJ_C_PTR_WRAPPER(FIRAppCheck);

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

  FutureManager& future_manager() { return future_manager_; }

  ReferenceCountedFutureImpl* future();

 private:
#ifdef __OBJC__
  FIRAppCheck* impl() const { return impl_->get(); }

  AppCheckNotificationCenterWrapper* notification_center_wrapper() const {
    return notification_center_wrapper_->get();
  }
#endif  // __OBJC__

  UniquePtr<FIRAppCheckPointer> impl_;

  UniquePtr<AppCheckNotificationCenterWrapperPointer> notification_center_wrapper_;

  ::firebase::App* app_;

  FutureManager future_manager_;
};

}  // namespace internal

}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_IOS_APP_CHECK_IOS_H_
