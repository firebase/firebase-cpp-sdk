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

#include <cstdint>
#include <string>

#include "app/src/include/firebase/internal/mutex.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "installations/src/common.h"

namespace firebase {
namespace installations {
namespace internal {

// Stub implementation of Installations for desktop builds.
//
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
  std::string ResolveTokenLocked(bool force_refresh);

  InstallationsFutures futures_;
  Mutex mutex_;
  uint64_t generation_ = 0;
  std::string cached_id_;
  std::string cached_token_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_INSTALLATIONS_SRC_STUB_INSTALLATIONS_STUB_H_
