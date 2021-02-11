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

#ifndef FIREBASE_FIS_CLIENT_CPP_SRC_IOS_INSTALLATIONS_IOS_H_
#define FIREBASE_FIS_CLIENT_CPP_SRC_IOS_INSTALLATIONS_IOS_H_

#include "firebase/app.h"
#include "app/memory/unique_ptr.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

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

  Future<std::string> GetId();
  Future<std::string> GetIdLastResult();

  Future<void> Delete();
  Future<void> DeleteLastResult();

  Future<std::string> GetToken(bool forceRefresh);
  Future<std::string> GetTokenLastResult();

  bool Initialized() const;

  void Cleanup();

 private:
#ifdef __OBJC__
  FIRInstallations* _Nullable impl() const { return impl_->get(); }
#endif
  // app
  const firebase::App& app_;

  UniquePtr<FIRInstallationsPointer> impl_;

  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_FIS_CLIENT_CPP_SRC_IOS_INSTALLATIONS_IOS_H_
