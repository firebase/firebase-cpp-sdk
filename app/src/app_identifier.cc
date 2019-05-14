/*
 * Copyright 2019 Google LLC
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

#include "app/src/app_identifier.h"

#include <string.h>

#include <string>

#include "app/src/include/firebase/app.h"

namespace FIREBASE_NAMESPACE {
namespace internal {

// Generate a unique identifier from the Firebase app options.
std::string CreateAppIdentifierFromOptions(const AppOptions& options) {
  // Combine bundle ID with project ID to create an app identifier.
  const char* package_name = options.package_name();
  const char* project_id = options.project_id();
  std::string app_identifier;

  if (strlen(package_name) > 0) app_identifier += package_name;

  if (strlen(project_id) > 0) {
    if (app_identifier.length() > 0) app_identifier += ".";
    app_identifier += project_id;
  }
  return app_identifier;
}

}  // namespace internal
}  // namespace FIREBASE_NAMESPACE
