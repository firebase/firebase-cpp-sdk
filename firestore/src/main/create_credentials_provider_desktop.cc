/*
 * Copyright 2021 Google LLC
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
#include "auth/src/include/firebase/auth.h"
#include "firestore/src/main/create_credentials_provider.h"
#include "firestore/src/main/credentials_provider_desktop.h"

namespace firebase {
namespace firestore {

using credentials::AuthCredentialsProvider;
using firebase::auth::Auth;

std::unique_ptr<AuthCredentialsProvider> CreateCredentialsProvider(App& app) {
  return absl::make_unique<FirebaseCppCredentialsProvider>(app);
}

}  // namespace firestore
}  // namespace firebase
