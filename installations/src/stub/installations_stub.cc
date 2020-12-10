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

#include "installations/src/stub/installations_stub.h"

#include "installations/src/common.h"

namespace firebase {
namespace installations {

namespace internal {

InstallationsInternal::InstallationsInternal(const firebase::App& app)
    : app_(app), future_impl_(kInstallationsFnCount) {}

InstallationsInternal::~InstallationsInternal() {}

bool InstallationsInternal::Initialized() const { return true; }

void InstallationsInternal::Cleanup() {}

Future<std::string> InstallationsInternal::GetId() {
  const auto handle =
      future_impl_.SafeAlloc<std::string>(kInstallationsFnGetId);
  future_impl_.CompleteWithResult(handle, kInstallationsErrorNone, "",
                                  std::string("FakeId"));

  return MakeFuture<std::string>(&future_impl_, handle);
}

Future<std::string> InstallationsInternal::GetIdLastResult() {
  return static_cast<const Future<std::string>&>(
      future_impl_.LastResult(kInstallationsFnGetId));
}

Future<std::string> InstallationsInternal::GetToken(bool forceRefresh) {
  const auto handle =
      future_impl_.SafeAlloc<std::string>(kInstallationsFnGetToken);

  future_impl_.CompleteWithResult(
      handle, kInstallationsErrorNone, "",
      forceRefresh  ? std::string("FakeToken2")  : std::string("FakeToken"));

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

}  // namespace internal
}  // namespace installations
}  // namespace firebase
