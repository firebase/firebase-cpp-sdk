//  Copyright (c) 2022 Google LLC
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

#include "installations/src/desktop/installations_desktop.h"

#include "installations/src/common.h"
#include "app/src/callback.h"
#include "app/src/log.h"

namespace firebase {
namespace installations {
namespace internal {

using callback::NewCallback;

template <typename T>
struct FisDataHandle {
  FisDataHandle(
      ReferenceCountedFutureImpl* _future_api,
      const SafeFutureHandle<T>& _future_handle,
      InstallationsInternal* _fis_internal)
      : future_api(_future_api),
        future_handle(_future_handle),
        fis_internal(_fis_internal) {}
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<T> future_handle;
  InstallationsInternal* fis_internal;
};

InstallationsInternal::InstallationsInternal(const firebase::App& app)
    : app_(app), 
      future_impl_(kInstallationsFnCount),
      safe_this_(this),
      rest_(app.options()) {
  SetLogLevel(kLogLevelVerbose);
}

InstallationsInternal::~InstallationsInternal() {}

void InstallationsInternal::LogHeartbeat(const firebase::App& app) {}

bool InstallationsInternal::Initialized() const { return true; }

void InstallationsInternal::Cleanup() {}

Future<std::string> InstallationsInternal::GetId() {
  const auto future_handle =
      future_impl_.SafeAlloc<std::string>(kInstallationsFnGetId);

  auto data_handle =
        MakeShared<FisDataHandle<std::string>>(&future_impl_, future_handle, this);

    auto callback = NewCallback(
        [](ThisRef ref, SharedPtr<FisDataHandle<std::string>> handle) {
            ThisRefLock lock(&ref);
            if (lock.GetReference() != nullptr) {
            MutexLock lock(handle->fis_internal->internal_mutex_);

            handle->fis_internal->RegisterInstallations();

            handle->future_api->CompleteWithResult(
                handle->future_handle, kInstallationsErrorNone, "",
                handle->fis_internal->GetFid());
            }
        },
        safe_this_, data_handle);

    scheduler_.Schedule(callback);
  
//   future_impl_.CompleteWithResult(handle, kInstallationsErrorNone, "",
//                                   std::string("FakeId"));

  return MakeFuture<std::string>(&future_impl_, future_handle);
}

Future<std::string> InstallationsInternal::GetIdLastResult() {
  return static_cast<const Future<std::string>&>(
      future_impl_.LastResult(kInstallationsFnGetId));
}

Future<std::string> InstallationsInternal::GetToken(bool forceRefresh) {
  const auto handle =
      future_impl_.SafeAlloc<std::string>(kInstallationsFnGetToken);

  future_impl_.CompleteWithResult(handle, kInstallationsErrorNone, "",
                                  forceRefresh
                                      ? std::string("FakeTokenForceRefresh")
                                      : std::string("FakeToken"));

  return MakeFuture<std::string>(&future_impl_, handle);
}

Future<std::string> InstallationsInternal::GetTokenLastResult() {
  return static_cast<const Future<std::string>&>(
      future_impl_.LastResult(kInstallationsFnGetToken));
}

Future<void> InstallationsInternal::Delete() {
  const auto handle = future_impl_.SafeAlloc<void>(kInstallationsFnGetId);
  future_impl_.Complete(handle, kInstallationsErrorNone);
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> InstallationsInternal::DeleteLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kInstallationsFnDelete));
}

void InstallationsInternal::RegisterInstallations() {
  rest_.RegisterInstallations(app_);
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
