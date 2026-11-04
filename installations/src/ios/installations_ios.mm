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

#include "installations/src/ios/installations_ios.h"

#include <cstdint>
#include <memory>
#include <string>

#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "installations/src/common.h"

#ifdef __OBJC__
#import "FIRInstallationsAuthTokenResult.h"
#import "FIRInstallationsErrors.h"
#endif  // __OBJC__

namespace firebase {
namespace installations {
namespace internal {

DEFINE_FIREBASE_VERSION_STRING(FirebaseInstallations);

namespace {
#ifdef __OBJC__
Error MapIosError(NSError* _Nullable error) {
  if (error == nil) return kInstallationsErrorNone;
  switch (error.code) {
    case FIRInstallationsErrorCodeKeychain:
      return kErrorUnableToLoad;
    case FIRInstallationsErrorCodeServerUnreachable:
      return kErrorNoAccess;
    case FIRInstallationsErrorCodeInvalidConfiguration:
      return kErrorInvalidConfiguration;
    default:
      return kInstallationsErrorFailure;
  }
}

template <InstallationsFn Slot, typename StartFn>
Future<SlotResultT<Slot>> RunIosOp(FIRInstallations* _Nullable inst,
                                   InstallationsFutures& futures,
                                   StartFn start_fn) {
  const auto handle = futures.SafeAlloc<Slot>();
  if (inst == nil) {
    futures.Complete<Slot>(handle, kErrorInvalidConfiguration,
                           "Installations is not initialized.");
    return futures.MakeFuture<Slot>(handle);
  }
  start_fn(inst, handle, futures.weak());
  return futures.MakeFuture<Slot>(handle);
}
#endif  // __OBJC__
}  // namespace

InstallationsInternal::InstallationsInternal(const firebase::App& app) {
  FIRApp* platform_app = app.GetPlatformApp();
  if (platform_app == nil) {
    LogError("Failed to initialize Installations: FIRApp is nil.");
    return;
  }
  @try {
    FIRInstallations* inst =
        [FIRInstallations installationsWithApp:platform_app];
    if (inst != nil) impl_ = std::make_unique<FIRInstallationsPointer>(inst);
  } @catch (NSException* exception) {
    LogError("Exception initializing FIRInstallations: %s",
             util::NSStringToString(exception.reason).c_str());
  }
}

InstallationsInternal::~InstallationsInternal() { Cleanup(); }

void InstallationsInternal::LogHeartbeat(const firebase::App& app) {
  FIRApp* platform_app = app.GetPlatformApp();
  if (platform_app == nil) return;
  @try {
    (void)[FIRInstallations installationsWithApp:platform_app];
  } @catch (NSException* exception) {
    LogWarning("Exception logging Installations heartbeat: %s",
               util::NSStringToString(exception.reason).c_str());
  }
}

bool InstallationsInternal::Initialized() const {
  return impl_ != nullptr && impl_->get() != nil;
}

void InstallationsInternal::Cleanup() { impl_.reset(); }

Future<std::string> InstallationsInternal::GetId() {
  return RunIosOp<kInstallationsFnGetId>(
      impl(), futures_,
      [](FIRInstallations* inst, SafeFutureHandle<std::string> h,
         std::weak_ptr<ReferenceCountedFutureImpl> weak_api) {
        [inst installationIDWithCompletion:^(NSString* _Nullable identity,
                                             NSError* _Nullable error) {
          if (auto api = weak_api.lock()) {
            if (error != nil) {
              api->Complete(
                  h, MapIosError(error),
                  util::NSStringToString(error.localizedDescription).c_str());
            } else {
              api->CompleteWithResult(
                  h, kInstallationsErrorNone, "",
                  identity ? util::NSStringToString(identity) : std::string());
            }
          }
        }];
      });
}

Future<std::string> InstallationsInternal::GetToken(bool force_refresh) {
  return RunIosOp<kInstallationsFnGetToken>(
      impl(), futures_,
      [force_refresh](FIRInstallations* inst, SafeFutureHandle<std::string> h,
                      std::weak_ptr<ReferenceCountedFutureImpl> weak_api) {
        [inst authTokenForcingRefresh:force_refresh
                           completion:^(FIRInstallationsAuthTokenResult* token,
                                        NSError* error) {
                             if (auto api = weak_api.lock()) {
                               if (error != nil) {
                                 api->Complete(
                                     h, MapIosError(error),
                                     util::NSStringToString(
                                         error.localizedDescription)
                                         .c_str());
                               } else if (token == nil ||
                                          token.authToken == nil) {
                                 api->Complete(h, kInstallationsErrorFailure,
                                               "Token result is nil.");
                               } else {
                                 api->CompleteWithResult(
                                     h, kInstallationsErrorNone, "",
                                     util::NSStringToString(token.authToken));
                               }
                             }
                           }];
      });
}

Future<InstallationTokenResult> InstallationsInternal::GetTokenResult(
    bool force_refresh) {
  return RunIosOp<kInstallationsFnGetTokenResult>(
      impl(), futures_,
      [force_refresh](FIRInstallations* inst,
                      SafeFutureHandle<InstallationTokenResult> h,
                      std::weak_ptr<ReferenceCountedFutureImpl> weak_api) {
        [inst authTokenForcingRefresh:force_refresh
                           completion:^(FIRInstallationsAuthTokenResult* token,
                                        NSError* error) {
                             if (auto api = weak_api.lock()) {
                               if (error != nil) {
                                 api->Complete(
                                     h, MapIosError(error),
                                     util::NSStringToString(
                                         error.localizedDescription)
                                         .c_str());
                               } else if (token == nil ||
                                          token.authToken == nil) {
                                 api->Complete(h, kInstallationsErrorFailure,
                                               "Token result is nil.");
                               } else {
                                 InstallationTokenResult res;
                                 res.token =
                                     util::NSStringToString(token.authToken);
                                 if (token.expirationDate != nil) {
                                   res.expiration_timestamp =
                                       static_cast<int64_t>(
                                           [token.expirationDate
                                               timeIntervalSince1970] *
                                           1000.0);
                                 }
                                 api->CompleteWithResult(
                                     h, kInstallationsErrorNone, "", res);
                               }
                             }
                           }];
      });
}

Future<void> InstallationsInternal::Delete() {
  return RunIosOp<kInstallationsFnDelete>(
      impl(), futures_,
      [](FIRInstallations* inst, SafeFutureHandle<void> h,
         std::weak_ptr<ReferenceCountedFutureImpl> weak_api) {
        [inst deleteWithCompletion:^(NSError* _Nullable error) {
          if (auto api = weak_api.lock()) {
            api->Complete(h, MapIosError(error),
                          error ? util::NSStringToString(
                                      error.localizedDescription)
                                      .c_str()
                                : "");
          }
        }];
      });
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
