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

namespace {
constexpr int64_t kStubTokenExpirationTimestampMs = 3600000LL;
}  // namespace

InstallationsInternal::InstallationsInternal(const firebase::App& app) {
  (void)app;
}

InstallationsInternal::~InstallationsInternal() = default;

void InstallationsInternal::LogHeartbeat(const firebase::App& app) {
  (void)app;
}

bool InstallationsInternal::Initialized() const { return true; }

void InstallationsInternal::Cleanup() {}

Future<std::string> InstallationsInternal::GetId() {
  std::string id;
  {
    MutexLock lock(mutex_);
    if (cached_id_.empty()) {
      cached_id_ = (generation_ == 0)
                       ? std::string("FakeId")
                       : (std::string("FakeId_") + std::to_string(generation_));
    }
    id = cached_id_;
  }
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetId>();
  futures_.CompleteWithResult<kInstallationsFnGetId>(
      handle, kInstallationsErrorNone, "", id);
  return futures_.MakeFuture<kInstallationsFnGetId>(handle);
}

Future<std::string> InstallationsInternal::GetIdLastResult() const {
  return futures_.LastResult<kInstallationsFnGetId>();
}

std::string InstallationsInternal::ResolveTokenLocked(bool force_refresh) {
  if (force_refresh) {
    cached_token_ = (generation_ == 0)
                        ? std::string("FakeTokenForceRefresh")
                        : (std::string("FakeTokenForceRefresh_") +
                           std::to_string(generation_));
  } else if (cached_token_.empty()) {
    cached_token_ =
        (generation_ == 0)
            ? std::string("FakeToken")
            : (std::string("FakeToken_") + std::to_string(generation_));
  }
  return cached_token_;
}

Future<std::string> InstallationsInternal::GetToken(bool force_refresh) {
  std::string token;
  {
    MutexLock lock(mutex_);
    token = ResolveTokenLocked(force_refresh);
  }
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetToken>();
  futures_.CompleteWithResult<kInstallationsFnGetToken>(
      handle, kInstallationsErrorNone, "", token);
  return futures_.MakeFuture<kInstallationsFnGetToken>(handle);
}

Future<std::string> InstallationsInternal::GetTokenLastResult() const {
  return futures_.LastResult<kInstallationsFnGetToken>();
}

Future<InstallationTokenResult> InstallationsInternal::GetTokenResult(
    bool force_refresh) {
  InstallationTokenResult token_result;
  {
    MutexLock lock(mutex_);
    token_result.token = ResolveTokenLocked(force_refresh);
    token_result.expiration_timestamp = kStubTokenExpirationTimestampMs;
  }
  const auto handle = futures_.SafeAlloc<kInstallationsFnGetTokenResult>();
  futures_.CompleteWithResult<kInstallationsFnGetTokenResult>(
      handle, kInstallationsErrorNone, "", token_result);
  return futures_.MakeFuture<kInstallationsFnGetTokenResult>(handle);
}

Future<InstallationTokenResult>
InstallationsInternal::GetTokenResultLastResult() const {
  return futures_.LastResult<kInstallationsFnGetTokenResult>();
}

Future<void> InstallationsInternal::Delete() {
  {
    MutexLock lock(mutex_);
    ++generation_;
    cached_id_.clear();
    cached_token_.clear();
  }
  const auto handle = futures_.SafeAlloc<kInstallationsFnDelete>();
  futures_.Complete<kInstallationsFnDelete>(handle, kInstallationsErrorNone,
                                            "");
  return futures_.MakeFuture<kInstallationsFnDelete>(handle);
}

Future<void> InstallationsInternal::DeleteLastResult() const {
  return futures_.LastResult<kInstallationsFnDelete>();
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase
