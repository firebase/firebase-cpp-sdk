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

#include "app/src/include/firebase/app.h"

#include <string.h>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

const char* const kDefaultAppName = "default";

void App::Initialize() {
  data_ = new internal::FunctionRegistry;
  LogDebug("Creating firebase::App for %s", kFirebaseVersionString);
}

App::~App() {
  app_common::RemoveApp(this);
  delete static_cast<internal::FunctionRegistry*>(data_);
}

App* App::Create() { return Create(AppOptions()); }

App* App::Create(const AppOptions& options) {
  return Create(options, kDefaultAppName);
}

App* App::Create(const AppOptions& options, const char* name) {
  App* existing_app = GetInstance(name);
  if (existing_app) {
    LogError("firebase::App %s already created, options will not be applied.",
             name);
    return existing_app;
  }
  App* app = new App();
  app->name_ = name;
  app->options_ = options;
  return app_common::AddApp(app, strcmp(name, kDefaultAppName) == 0,
                            &app->init_results_);
}

App* App::GetInstance() { return app_common::GetDefaultApp(); }

App* App::GetInstance(const char* name) {
  return app_common::FindAppByName(name);
}

#ifdef INTERNAL_EXPERIMENTAL
internal::FunctionRegistry* App::function_registry() {
  return static_cast<internal::FunctionRegistry*>(data_);
}
#endif

void App::RegisterLibrary(const char* library, const char* version) {
  app_common::RegisterLibrary(library, version);
}

const char* App::GetUserAgent() { return app_common::GetUserAgent(); }

void App::SetDefaultConfigPath(const char* /* path */) {}

void App::SetDataCollectionDefaultEnabled(bool /* enabled */) {}

bool App::IsDataCollectionDefaultEnabled() const { return true; }

}  // namespace firebase
