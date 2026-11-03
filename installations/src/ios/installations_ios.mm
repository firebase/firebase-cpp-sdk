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
  if (error == nil) {
    return kInstallationsErrorNone;
  }
  switch (error.code) {
    case FIRInstallationsErrorCodeKeychain:
      return kErrorUnableToLoad;
    case FIRInstallationsErrorCodeServerUnreachable:
      return kErrorNoAccess;
    case FIRInstallationsErrorCodeInvalidConfiguration:
      return kErrorInvalidConfiguration;
    case FIRInstallationsErrorCodeUnknown:
    default:
      return kInstallationsErrorFailure;
  }
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
    FIRInstallations* installations =
        [FIRInstallations installationsWithApp:platform_app];
    if (installations != nil) {
      impl_ = std::make_unique<FIRInstallationsPointer>(installations);
    }
  } @catch (NSException* exception) {
    LogError("Exception initializing FIRInstallations: %s",
             util::NSStringToString(exception.reason).c_str());
  }
}

InstallationsInternal::~InstallationsInternal() {
  // Destructor is necessary for ARC garbage collection.
  Cleanup();
}

void InstallationsInternal::LogHeartbeat(const firebase::App& app) {
  // Calling the native getter is sufficient to cause a Heartbeat to be logged.
  FIRApp* platform_app = app.GetPlatformApp();
  if (platform_app == nil) {
    return;
  }
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
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetId>();
  FIRInstallations* installations = impl();
  if (installations == nil) {
    futures_.Complete<kInstallationsFnGetId>(
        handle, kErrorInvalidConfiguration,
        "Installations is not initialized.");
    return futures_.MakeFuture<kInstallationsFnGetId>(handle);
  }
  std::weak_ptr<ReferenceCountedFutureImpl> weak_future = futures_.weak();
  [installations
      installationIDWithCompletion:^(NSString* _Nullable identity,
                                     NSError* _Nullable error) {
        auto api = weak_future.lock();
        if (!api) {
          return;
        }
        if (error != nil) {
          api->Complete(
              handle, MapIosError(error),
              util::NSStringToString(error.localizedDescription).c_str());
        } else {
          api->CompleteWithResult(
              handle, kInstallationsErrorNone, "",
              identity != nil ? util::NSStringToString(identity)
                              : std::string());
        }
      }];
  return futures_.MakeFuture<kInstallationsFnGetId>(handle);
}

Future<std::string> InstallationsInternal::GetIdLastResult() const {
  return futures_.LastResult<kInstallationsFnGetId>();
}

Future<std::string> InstallationsInternal::GetToken(bool force_refresh) {
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetToken>();
  FIRInstallations* installations = impl();
  if (installations == nil) {
    futures_.Complete<kInstallationsFnGetToken>(
        handle, kErrorInvalidConfiguration,
        "Installations is not initialized.");
    return futures_.MakeFuture<kInstallationsFnGetToken>(handle);
  }
  std::weak_ptr<ReferenceCountedFutureImpl> weak_future = futures_.weak();
  FIRInstallationsTokenHandler token_handler =
      ^(FIRInstallationsAuthTokenResult* _Nullable token,
        NSError* _Nullable error) {
        auto api = weak_future.lock();
        if (!api) {
          return;
        }
        if (error != nil) {
          api->Complete(
              handle, MapIosError(error),
              util::NSStringToString(error.localizedDescription).c_str());
        } else if (token == nil || token.authToken == nil) {
          api->Complete(handle, kInstallationsErrorFailure,
                        "Token result is nil.");
        } else {
          api->CompleteWithResult(handle, kInstallationsErrorNone, "",
                                  util::NSStringToString(token.authToken));
        }
      };

  [installations authTokenForcingRefresh:force_refresh
                              completion:token_handler];

  return futures_.MakeFuture<kInstallationsFnGetToken>(handle);
}

Future<std::string> InstallationsInternal::GetTokenLastResult() const {
  return futures_.LastResult<kInstallationsFnGetToken>();
}

Future<InstallationTokenResult> InstallationsInternal::GetTokenResult(
    bool force_refresh) {
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetTokenResult>();
  FIRInstallations* installations = impl();
  if (installations == nil) {
    futures_.Complete<kInstallationsFnGetTokenResult>(
        handle, kErrorInvalidConfiguration,
        "Installations is not initialized.");
    return futures_.MakeFuture<kInstallationsFnGetTokenResult>(handle);
  }
  std::weak_ptr<ReferenceCountedFutureImpl> weak_future = futures_.weak();
  FIRInstallationsTokenHandler token_handler =
      ^(FIRInstallationsAuthTokenResult* _Nullable token,
        NSError* _Nullable error) {
        auto api = weak_future.lock();
        if (!api) {
          return;
        }
        if (error != nil) {
          api->Complete(
              handle, MapIosError(error),
              util::NSStringToString(error.localizedDescription).c_str());
        } else if (token == nil || token.authToken == nil) {
          api->Complete(handle, kInstallationsErrorFailure,
                        "Token result is nil.");
        } else {
          InstallationTokenResult result;
          result.token = util::NSStringToString(token.authToken);
          if (token.expirationDate != nil) {
            result.expiration_timestamp = static_cast<int64_t>(
                [token.expirationDate timeIntervalSince1970] * 1000.0);
          }
          api->CompleteWithResult(handle, kInstallationsErrorNone, "", result);
        }
      };

  [installations authTokenForcingRefresh:force_refresh
                              completion:token_handler];

  return futures_.MakeFuture<kInstallationsFnGetTokenResult>(handle);
}

Future<InstallationTokenResult>
InstallationsInternal::GetTokenResultLastResult() const {
  return futures_.LastResult<kInstallationsFnGetTokenResult>();
}

Future<void> InstallationsInternal::Delete() {
  const auto handle = futures_.SafeAlloc<kInstallationsFnDelete>();
  FIRInstallations* installations = impl();
  if (installations == nil) {
    futures_.Complete<kInstallationsFnDelete>(
        handle, kErrorInvalidConfiguration,
        "Installations is not initialized.");
    return futures_.MakeFuture<kInstallationsFnDelete>(handle);
  }
  std::weak_ptr<ReferenceCountedFutureImpl> weak_future = futures_.weak();
  [installations deleteWithCompletion:^(NSError* _Nullable error) {
    auto api = weak_future.lock();
    if (!api) {
      return;
    }
    if (error != nil) {
      api->Complete(handle, MapIosError(error),
                    util::NSStringToString(error.localizedDescription).c_str());
    } else {
      api->Complete(handle, kInstallationsErrorNone, "");
    }
  }];
  return futures_.MakeFuture<kInstallationsFnDelete>(handle);
}

Future<void> InstallationsInternal::DeleteLastResult() const {
  return futures_.LastResult<kInstallationsFnDelete>();
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
