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

#include <string>

#include "installations/src/common.h"

namespace firebase {
namespace installations {
namespace internal {

InstallationsInternal::InstallationsInternal(const firebase::App&) {}
InstallationsInternal::~InstallationsInternal() = default;
void InstallationsInternal::LogHeartbeat(const firebase::App&) {}

Future<std::string> InstallationsInternal::GetId() {
  std::string id;
  {
    MutexLock lock(mutex_);
    if (cached_id_.empty()) {
      cached_id_ = generation_ == 0
                       ? "FakeId"
                       : ("FakeId_" + std::to_string(generation_));
    }
    id = cached_id_;
  }
  const auto h = futures_.SafeAlloc<kInstallationsFnGetId>();
  futures_.CompleteWithResult<kInstallationsFnGetId>(h, kInstallationsErrorNone,
                                                     "", id);
  return futures_.MakeFuture<kInstallationsFnGetId>(h);
}

std::string InstallationsInternal::ResolveTokenLocked(bool force_refresh) {
  if (force_refresh) {
    cached_token_ =
        generation_ == 0
            ? "FakeTokenForceRefresh"
            : ("FakeTokenForceRefresh_" + std::to_string(generation_));
  } else if (cached_token_.empty()) {
    cached_token_ = generation_ == 0
                        ? "FakeToken"
                        : ("FakeToken_" + std::to_string(generation_));
  }
  return cached_token_;
}

Future<std::string> InstallationsInternal::GetToken(bool force_refresh) {
  std::string token;
  {
    MutexLock lock(mutex_);
    token = ResolveTokenLocked(force_refresh);
  }
  const auto h = futures_.SafeAlloc<kInstallationsFnGetToken>();
  futures_.CompleteWithResult<kInstallationsFnGetToken>(
      h, kInstallationsErrorNone, "", token);
  return futures_.MakeFuture<kInstallationsFnGetToken>(h);
}

Future<InstallationTokenResult> InstallationsInternal::GetTokenResult(
    bool force_refresh) {
  InstallationTokenResult res;
  {
    MutexLock lock(mutex_);
    res.token = ResolveTokenLocked(force_refresh);
    res.expiration_timestamp = 3600000LL;
  }
  const auto h = futures_.SafeAlloc<kInstallationsFnGetTokenResult>();
  futures_.CompleteWithResult<kInstallationsFnGetTokenResult>(
      h, kInstallationsErrorNone, "", res);
  return futures_.MakeFuture<kInstallationsFnGetTokenResult>(h);
}

Future<void> InstallationsInternal::Delete() {
  {
    MutexLock lock(mutex_);
    ++generation_;
    cached_id_.clear();
    cached_token_.clear();
  }
  const auto h = futures_.SafeAlloc<kInstallationsFnDelete>();
  futures_.Complete<kInstallationsFnDelete>(h, kInstallationsErrorNone, "");
  return futures_.MakeFuture<kInstallationsFnDelete>(h);
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
