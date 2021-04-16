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
#ifndef FIREBASE_FIS_CLIENT_CPP_SRC_STUB_INSTALLATIONS_STUB_H_
#define FIREBASE_FIS_CLIENT_CPP_SRC_STUB_INSTALLATIONS_STUB_H_

#include "app/src/reference_counted_future_impl.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace installations {
namespace internal {
// Installations Client implementation for Android.
//
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
  const firebase::App& app_;  // NOLINT

  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_FIS_CLIENT_CPP_SRC_STUB_INSTALLATIONS_STUB_H_
