/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <string>
#include <string_view>

#include "app/src/app_common.h"
#include "app/src/app_state.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace internal {

void AppState::DestroyPlatformData() {}

}  // namespace internal

App App::Create() { return Create(AppOptions()); }

App App::Create(const AppOptions& options) {
  return Create(options, kDefaultAppName);
}

App App::Create(const AppOptions& options, std::string_view name) {
  LogDebug("Creating firebase::App for %s", kFirebaseVersionString);
  auto state = std::make_shared<internal::AppState>(std::string(name), options);
  App app = state->MakeAppHandle();
  AppCallback::NotifyAllAppCreated(app, &state->mutable_init_results());
  return app;
}

App App::CreateRegistered() { return CreateRegistered(AppOptions()); }

App App::CreateRegistered(const AppOptions& options) {
  return CreateRegistered(options, kDefaultAppName);
}

App App::CreateRegistered(const AppOptions& options, std::string_view name) {
  return app_common::CreateRegisteredApp(
      name, [&]() { return Create(options, name); });
}

void App::RegisterLibrary(const char* library, const char* version,
                          void* /* platform_resource */) {
  app_common::RegisterLibrary(library, version);
}

void App::SetDefaultConfigPath(const char* /* path */) {}

void App::SetDataCollectionDefaultEnabled(bool /* enabled */) {}

bool App::IsDataCollectionDefaultEnabled() const { return true; }

}  // namespace firebase
