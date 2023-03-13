/*
 * Copyright 2023 Google LLC
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

#include "absl/memory/memory.h"
#include "firestore/src/main/app_check_credentials_provider_desktop.h"
#include "firestore/src/main/create_credentials_provider.h"

namespace firebase {
namespace firestore {

using credentials::AppCheckCredentialsProvider;

std::unique_ptr<AppCheckCredentialsProvider> CreateAppCheckCredentialsProvider(
    App& app) {
  return absl::make_unique<CppAppCheckCredentialsProvider>(app);
}

}  // namespace firestore
}  // namespace firebase
