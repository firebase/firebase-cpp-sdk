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

#include <cstdint>
#include <string>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#import "FIRInstanceID.h"

#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_ios.h"
#include "instance_id/src/include/firebase/instance_id.h"
#include "instance_id/src/instance_id_internal.h"

namespace firebase {
namespace instance_id {

using internal::InstanceIdInternal;

namespace {

// Maps a FIRInstanceIDError to an error code.
// NOTE: IID actually sets error codes using FIRInstanceIDError which just so happen to map to the
// error code values in FIRInstanceIDError.  All other error numbers are opaque.
struct IIDErrorToCode {
  FIRInstanceIDError nserror_code;
  Error code;
};

// Device token cached by UIApplication(FIRIID), required to fetch tokens from IID.
static NSData* g_device_token = nil;

// Convert a NSError to a error code.
static Error NSErrorToErrorCode(NSError *_Nullable nserror) {
  static const IIDErrorToCode kIIDErrorsToCodes[] = {
    { FIRInstanceIDErrorUnknown, kErrorUnknown },
    { FIRInstanceIDErrorAuthentication, kErrorAuthentication },
    { FIRInstanceIDErrorNoAccess, kErrorNoAccess },
    { FIRInstanceIDErrorTimeout, kErrorTimeout },
    { FIRInstanceIDErrorNetwork, kErrorNetwork },
    { FIRInstanceIDErrorOperationInProgress, kErrorOperationInProgress },
    { FIRInstanceIDErrorInvalidRequest, kErrorInvalidRequest },
  };
  if (nserror) {
    NSInteger nserror_code = nserror.code;
    for (int i = 0; i < FIREBASE_ARRAYSIZE(kIIDErrorsToCodes); ++i) {
      const auto& iid_error_to_code = kIIDErrorsToCodes[i];
      if (iid_error_to_code.nserror_code == nserror_code) {
        return iid_error_to_code.code;
      }
    }
    return kErrorUnknown;
  }
  return kErrorNone;
}

// Complete an operation with no result.
static void CompleteOperation(
    FIRInstanceIdInternalOperation* _Nonnull operation,
    const SafeFutureHandle<void>& future_handle,
    NSError* _Nullable error) {
  InstanceIdInternal* iid_internal = [operation start];
  if (iid_internal) {
    iid_internal->future_api().Complete(
        future_handle, NSErrorToErrorCode(error),
        util::NSStringToString(error.localizedDescription).c_str());
  }
  [operation end];
}

// Complete an operation with a result.
template<typename T>
static void CompleteOperationWithResult(
    FIRInstanceIdInternalOperation* _Nonnull operation,
    const SafeFutureHandle<T>& future_handle,
    NSError* _Nullable error,
    const T& result) {
  InstanceIdInternal* iid_internal = [operation start];
  if (iid_internal) {
    iid_internal->future_api().CompleteWithResult(
        future_handle, NSErrorToErrorCode(error),
        util::NSStringToString(error.localizedDescription).c_str(),
        result);
  }
  [operation end];
}

// Get the swizzled method cached for this module.
static util::ClassMethodImplementationCache& SwizzledMethodCache() {
  static util::ClassMethodImplementationCache *g_swizzled_method_cache;
  return *util::ClassMethodImplementationCache::GetCreateCache(
       &g_swizzled_method_cache);
}

// Caches the device token required to fetch IID tokens.
static void AppDelegateApplicationDidRegisterForRemoteNotificationsWithDeviceToken(
    id self, SEL selector_value, UIApplication *application, NSData *device_token) {
  LogDebug("Caching device token");
  g_device_token = device_token;
  IMP app_delegate_application_did_register_for_remote_notifications_with_device_token =
      SwizzledMethodCache().GetMethodForObject(
          self, @selector(application:didRegisterForRemoteNotificationsWithDeviceToken:));
  if (app_delegate_application_did_register_for_remote_notifications_with_device_token) {
    ((util::AppDelegateApplicationDidRegisterForRemoteNotificationsWithDeviceTokenFunc)
         app_delegate_application_did_register_for_remote_notifications_with_device_token)(
        self, selector_value, application, device_token);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature* signature = [[self class] instanceMethodSignatureForSelector:selector_value];
    NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector_value];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&device_token atIndex:3];
    [self forwardInvocation:invocation];
  }
}
// Hook all AppDelegate methods that IID requires to cache data for token fetch operations.
//
// The user of the library provides the class which implements AppDelegate protocol so this
// method hooks methods of user's UIApplication class in order to intercept events required for
// IID.  The alternative to this procedure would require the user to implement boilerplate code
// in their UIApplication in order to plumb in IID.
//
// The following methods are replaced in order to intercept AppDelegate events:
// * application:didRegisterForRemoteNotificationsWithDeviceToken:
static void HookAppDelegateMethods(Class clazz) {
  Class method_encoding_class = [FIRSAMAppDelegate class];
  auto& method_cache = SwizzledMethodCache();
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(application:didRegisterForRemoteNotificationsWithDeviceToken:),
      (IMP)AppDelegateApplicationDidRegisterForRemoteNotificationsWithDeviceToken,
      method_encoding_class);
}

}  // namespace

int64_t InstanceId::creation_time() const {
  // TODO(b/69932424): iOS instance ID API is missing creation time.  This is hidden from the public
  // C++ interface until this is implemented.
  return 0;
}

Future<std::string> InstanceId::GetId() const {
  if (!instance_id_internal_) return Future<std::string>();

  const auto future_handle = instance_id_internal_->FutureAlloc<std::string>(
      InstanceIdInternal::kApiFunctionGetId);
  FIRInstanceID* fir_instance_id = instance_id_internal_->GetFIRInstanceID();
  FIRInstanceIdInternalOperation* operation =
      instance_id_internal_->AddOperation();
  [fir_instance_id getIDWithHandler:^(NSString *_Nullable identity,
                                      NSError *_Nullable error) {
      CompleteOperationWithResult(operation, future_handle, error,
                                  util::NSStringToString(identity));
    }];
  return GetIdLastResult();
}

Future<void> InstanceId::DeleteId() {
  if (!instance_id_internal_) return Future<void>();

  const auto future_handle = instance_id_internal_->FutureAlloc<void>(
      InstanceIdInternal::kApiFunctionDeleteId);
  FIRInstanceID* fir_instance_id = instance_id_internal_->GetFIRInstanceID();
  FIRInstanceIdInternalOperation* operation =
      instance_id_internal_->AddOperation();
  [fir_instance_id deleteIDWithHandler:^(NSError *_Nullable error) {
      CompleteOperation(operation, future_handle, error);
    }];
  return DeleteIdLastResult();
}

Future<std::string> InstanceId::GetToken(const char* entity,
                                         const char* scope) {
  if (!instance_id_internal_) return Future<std::string>();

  const auto future_handle = instance_id_internal_->FutureAlloc<std::string>(
      InstanceIdInternal::kApiFunctionGetToken);
  FIRInstanceID* fir_instance_id = instance_id_internal_->GetFIRInstanceID();
  FIRInstanceIdInternalOperation* operation =
      instance_id_internal_->AddOperation();
  // The `apns_token` key must have an associated value.  The value for the `apns_token` should be
  // the NSData object passed to the UIApplicationDelegate's
  // `didRegisterForRemoteNotificationsWithDeviceToken` method.
  // If there is no token, the options dictionary should be empty.
  NSDictionary* options = g_device_token ? @{@"apns_token" : g_device_token} : @{};
  [fir_instance_id tokenWithAuthorizedEntity:@(entity)
                                       scope:@(scope)
                                     options:options
                                     handler:^(NSString *_Nullable token,
                                               NSError *_Nullable error) {
      CompleteOperationWithResult(operation, future_handle, error,
                                  util::NSStringToString(token));
    }];
  return GetTokenLastResult();
}

Future<void> InstanceId::DeleteToken(const char* entity,
                                     const char* scope) {
  if (!instance_id_internal_) return Future<void>();

  const auto future_handle = instance_id_internal_->FutureAlloc<void>(
      InstanceIdInternal::kApiFunctionDeleteToken);
  FIRInstanceID* fir_instance_id = instance_id_internal_->GetFIRInstanceID();
  FIRInstanceIdInternalOperation* operation =
      instance_id_internal_->AddOperation();
  [fir_instance_id deleteTokenWithAuthorizedEntity:@(entity)
                                             scope:@(scope)
                                           handler:^(NSError *_Nullable error) {
      CompleteOperation(operation, future_handle, error);
    }];
  return DeleteTokenLastResult();
}

InstanceId* InstanceId::GetInstanceId(App* app, InitResult* init_result_out) {
  FIREBASE_ASSERT_MESSAGE_RETURN(nullptr, app, "App must be specified.");
  MutexLock lock(InstanceIdInternal::mutex());
  if (app != App::GetInstance()) {
    LogError("InstanceId can only be created for the default app on iOS. "
             "App %s is not the default app.", app->name());
    return nullptr;
  }
  if (init_result_out) *init_result_out = kInitResultSuccess;
  auto instance_id = InstanceIdInternal::FindInstanceIdByApp(app);
  if (instance_id) return instance_id;
  FIRInstanceID* fir_instance_id = FIRInstanceID.instanceID;
  if (!fir_instance_id) {
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return nullptr;
  }
  return new InstanceId(app, new InstanceIdInternal(
      new internal::FIRInstanceIDPointer(FIRInstanceID.instanceID)));
}

}  // namespace instance_id
}  // namespace firebase

// Category for UIApplication that is used to hook methods in all classes.
// Category +load() methods are called after all class load methods in each Mach-O
// (see call_load_methods() in
// http://www.opensource.apple.com/source/objc4/objc4-274/runtime/objc-runtime.m)
@implementation UIApplication (FIRIID)
+ (void)load {
  firebase::LogInfo("FIID: Loading UIApplication FIRIID category");
  firebase::util::ForEachAppDelegateClass(^(Class clazz) {
      firebase::instance_id::HookAppDelegateMethods(clazz);
  });
}
@end

