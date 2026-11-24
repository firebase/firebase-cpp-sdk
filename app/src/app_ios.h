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

#ifndef FIREBASE_APP_SRC_APP_IOS_H_
#define FIREBASE_APP_SRC_APP_IOS_H_

#include <optional>
#include <string_view>

#include "FIRApp.h"
#include "FIRConfiguration.h"
#include "app/src/include/firebase/app.h"
#include "app/src/util_ios.h"

namespace firebase {

namespace internal {
OBJ_C_PTR_WRAPPER_NAMED(AppInternal, FIRApp);

void SetFirConfigurationLoggerLevel(FIRLoggerLevel level);

// Looks up an App handle for an iOS FIRApp during native Objective-C callbacks
// (such as FIRAppCheckProviderFactory::createProviderWithApp:) regardless of
// whether the caller opted into global App::Register() name lookup, including
// while [FIRApp configureWithOptions:] is still executing inside App::Create().
std::optional<App> FindIosAppByName(std::string_view name);

}  // namespace internal

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APP_IOS_H_
