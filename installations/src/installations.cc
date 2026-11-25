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

#include "installations/src/include/firebase/installations.h"

#include <memory>
#include <utility>

#include "app/src/app_state.h"
#include "installations/src/installations_internal.h"

namespace firebase {
namespace installations {

using internal::InstallationsInternal;

namespace {
constexpr int kInstallationsServiceTag = 0;
}  // namespace

Installations::Installations(
    std::shared_ptr<internal::InstallationsInternal> internal)
    : installations_internal_(std::move(internal)) {}

Installations::~Installations() = default;

App Installations::app() const {
  return installations_internal_ ? installations_internal_->app() : App();
}

Installations Installations::GetInstance(const App& app) {
  if (!app.is_valid()) return {};

  const auto& state = ::firebase::internal::AppState::FromApp(app);
  if (!state) return {};

  bool created = false;
  auto internal = state->GetOrCreateService<InstallationsInternal>(
      &kInstallationsServiceTag, {},
      [&created](::firebase::internal::AppState& app_state)
          -> std::shared_ptr<InstallationsInternal> {
        LogDebug("Creating Installations for App %s", app_state.name().c_str());
        auto instance = std::make_shared<InstallationsInternal>(app_state);
        if (!instance->Initialized()) {
          return {};
        }
        created = true;
        return instance;
      });
  if (!internal) return {};

  if (!created) {
    // Log heartbeat data when instance getters are called.
    InstallationsInternal::LogHeartbeat(app);
  }
  return Installations(std::move(internal));
}

Future<std::string> Installations::GetId() {
  if (!installations_internal_) return {};
  return installations_internal_->GetId();
}

Future<std::string> Installations::GetIdLastResult() {
  if (!installations_internal_) return {};
  return installations_internal_->GetIdLastResult();
}

Future<std::string> Installations::GetToken(bool forceRefresh) {
  if (!installations_internal_) return {};
  return installations_internal_->GetToken(forceRefresh);
}

Future<std::string> Installations::GetTokenLastResult() {
  if (!installations_internal_) return {};
  return installations_internal_->GetTokenLastResult();
}

Future<void> Installations::Delete() {
  if (!installations_internal_) return {};
  return installations_internal_->Delete();
}

Future<void> Installations::DeleteLastResult() {
  if (!installations_internal_) return {};
  return installations_internal_->DeleteLastResult();
}

}  // namespace installations
}  // namespace firebase
