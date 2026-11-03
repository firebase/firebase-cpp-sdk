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

#ifndef FIREBASE_INSTALLATIONS_SRC_IOS_INSTALLATIONS_IOS_H_
#define FIREBASE_INSTALLATIONS_SRC_IOS_INSTALLATIONS_IOS_H_

#include <memory>
#include <string>

#include "app/src/util_ios.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "installations/src/common.h"

#ifdef __OBJC__
#import "FIRInstallations.h"
#endif  // __OBJC__

namespace firebase {
namespace installations {
namespace internal {
// Installations Client implementation for iOS.

// This defines the class FIRInstallationsPointer, which is a C++-compatible
// wrapper around the FIRInstallations Obj-C class.
OBJ_C_PTR_WRAPPER(FIRInstallations);

// This class implements functions from `firebase/installations.h` header.
// See `firebase/installations.h` for all public functions documentation.
class InstallationsInternal {
 public:
  explicit InstallationsInternal(const firebase::App& app);
  ~InstallationsInternal();

  InstallationsInternal(const InstallationsInternal&) = delete;
  InstallationsInternal& operator=(const InstallationsInternal&) = delete;

  // Platform-specific method that causes a heartbeat to be logged.
  // See go/firebase-platform-logging-design for more information.
  static void LogHeartbeat(const firebase::App& app);

  Future<std::string> GetId();
  Future<std::string> GetIdLastResult() const;

  Future<void> Delete();
  Future<void> DeleteLastResult() const;

  Future<std::string> GetToken(bool force_refresh);
  Future<std::string> GetTokenLastResult() const;

  Future<InstallationTokenResult> GetTokenResult(bool force_refresh);
  Future<InstallationTokenResult> GetTokenResultLastResult() const;

  bool Initialized() const;

  void Cleanup();

 private:
#ifdef __OBJC__
  FIRInstallations* _Nullable impl() const {
    return impl_ ? impl_->get() : nil;
  }
#endif

  std::unique_ptr<FIRInstallationsPointer> impl_;

  /// Handle calls from Futures that the API returns.
  InstallationsFutures futures_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_INSTALLATIONS_SRC_IOS_INSTALLATIONS_IOS_H_
