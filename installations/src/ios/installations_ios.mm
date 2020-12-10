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

#include "app/src/include/firebase/version.h"
#include "app/src/time.h"
#include "installations/src/common.h"

#ifdef __OBJC__
#import "FIRInstallationsAuthTokenResult.h"
#endif  // __OBJC__

namespace firebase {
namespace installations {

namespace internal {

DEFINE_FIREBASE_VERSION_STRING(FirebaseInstallations);

InstallationsInternal::InstallationsInternal(const firebase::App& app)
    : app_(app),
      future_impl_(kInstallationsFnCount) {
  FIRApp *platform_app = app_.GetPlatformApp();
  impl_.reset(new FIRInstallationsPointer([FIRInstallations installationsWithApp:platform_app]));
}

InstallationsInternal::~InstallationsInternal() {
  // Destructor is necessary for ARC garbage collection.
}

bool InstallationsInternal::Initialized() const{
  return true;
}

void InstallationsInternal::Cleanup() {

}

Future<std::string> InstallationsInternal::GetId() {
  const auto handle =
      future_impl_.SafeAlloc<std::string>(kInstallationsFnGetId);
  [impl() installationIDWithCompletion:^(NSString *_Nullable identity,
                                           NSError *_Nullable error) {
    if (error) {
      future_impl_.Complete(handle, kInstallationsErrorFailure,
                            util::NSStringToString(error.localizedDescription).c_str());
    } else {
      future_impl_.CompleteWithResult(handle, kInstallationsErrorNone,
                                      "test", util::NSStringToString(identity));
    }
  }];
  return MakeFuture<std::string>(&future_impl_, handle);
}

Future<std::string> InstallationsInternal::GetIdLastResult() {
  return static_cast<const Future<std::string>&>(
      future_impl_.LastResult(kInstallationsFnGetId));
}

Future<std::string> InstallationsInternal::GetToken(bool forceRefresh) {
  const auto handle =
      future_impl_.SafeAlloc<std::string>(kInstallationsFnGetToken);

  FIRInstallationsTokenHandler tokenHandler =
      ^(FIRInstallationsAuthTokenResult *_Nullable token,
        NSError *_Nullable error) {
    if (error) {
      future_impl_.Complete(handle, kInstallationsErrorFailure,
                            util::NSStringToString(error.localizedDescription).c_str());
    } else {
      future_impl_.CompleteWithResult(handle, kInstallationsErrorNone,
                                      "", util::NSStringToString(token.authToken));
    }
  };

  [impl() authTokenForcingRefresh:forceRefresh
                       completion:tokenHandler];

  return MakeFuture<std::string>(&future_impl_, handle);
}

Future<std::string> InstallationsInternal::GetTokenLastResult() {
  return static_cast<const Future<std::string>&>(
      future_impl_.LastResult(kInstallationsFnGetToken));
}

Future<void> InstallationsInternal::Delete() {
  const auto handle =
      future_impl_.SafeAlloc<void>(kInstallationsFnGetId);
  [impl() deleteWithCompletion:^(NSError *_Nullable error) {
    future_impl_.Complete(handle,
                          error == nullptr ? kInstallationsErrorNone : kInstallationsErrorFailure,
                          util::NSStringToString(error.localizedDescription).c_str());
  }];
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> InstallationsInternal::DeleteLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kInstallationsFnDelete));
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
