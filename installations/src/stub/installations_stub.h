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
#ifndef FIREBASE_INSTALLATIONS_SRC_STUB_INSTALLATIONS_STUB_H_
#define FIREBASE_INSTALLATIONS_SRC_STUB_INSTALLATIONS_STUB_H_

#include "app/src/app_state.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/service_interfaces.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace installations {
namespace internal {
// Installations Client stub implementation.
//
// This class implements functions from `firebase/installations.h` header.
// See `firebase/installations.h` for all public functions documentation.
class InstallationsInternal : public ::firebase::internal::Service {
 public:
  explicit InstallationsInternal(::firebase::internal::AppState& app_state);
  ~InstallationsInternal() override;

  // Platform-specific method that causes a heartbeat to be logged.
  // See go/firebase-platform-logging-design for more information.
  static void LogHeartbeat(const firebase::App& app);

  firebase::App app() const { return app_state_.TryMakeAppHandle(); }

  Future<std::string> GetId();
  Future<std::string> GetIdLastResult();

  Future<void> Delete();
  Future<void> DeleteLastResult();

  Future<std::string> GetToken(bool forceRefresh);
  Future<std::string> GetTokenLastResult();

  bool Initialized() const;

 private:
  ::firebase::internal::AppState& app_state_;

  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_INSTALLATIONS_SRC_STUB_INSTALLATIONS_STUB_H_
