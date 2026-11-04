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

#ifndef FIREBASE_INSTALLATIONS_SRC_ANDROID_INSTALLATIONS_ANDROID_H_
#define FIREBASE_INSTALLATIONS_SRC_ANDROID_INSTALLATIONS_ANDROID_H_

#include <jni.h>
#include <memory>
#include <string>

#include "firebase/app.h"
#include "firebase/future.h"
#include "installations/src/common.h"

namespace firebase {
namespace installations {
namespace internal {

struct PendingCallbacks;

class InstallationsInternal {
 public:
  explicit InstallationsInternal(const firebase::App& app);
  ~InstallationsInternal();

  InstallationsInternal(const InstallationsInternal&) = delete;
  InstallationsInternal& operator=(const InstallationsInternal&) = delete;

  static void LogHeartbeat(const firebase::App& app);

  Future<std::string> GetId();
  Future<void> Delete();
  Future<std::string> GetToken(bool force_refresh);
  Future<InstallationTokenResult> GetTokenResult(bool force_refresh);

  bool Initialized() const { return internal_obj_ != nullptr; }
  void Cleanup();
  const InstallationsFutures& futures() const { return futures_; }

 private:
  const firebase::App& app_;
  InstallationsFutures futures_;
  std::shared_ptr<PendingCallbacks> pending_callbacks_;
  jobject internal_obj_ = nullptr;
  bool has_initializer_ref_ = false;
  std::string jni_task_id_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_INSTALLATIONS_SRC_ANDROID_INSTALLATIONS_ANDROID_H_
