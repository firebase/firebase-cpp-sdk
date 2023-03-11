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

#include "firestore/src/main/create_credentials_provider.h"

#import "FIRAppInternal.h"
#import "FIRAuthInterop.h"
#import "FIRComponent.h"
#import "FIRComponentContainer.h"
#import "FIRComponentType.h"
#import "FirebaseCore.h"

#include "Firestore/core/src/credentials/firebase_app_check_credentials_provider_apple.h"
#include "absl/memory/memory.h"
#include "app/src/include/firebase/app.h"

namespace firebase {
namespace firestore {

using credentials::AppCheckCredentialsProvider;
using credentials::FirebaseAppCheckCredentialsProvider;

std::unique_ptr<AppCheckCredentialsProvider> CreateAppCheckCredentialsProvider(
    App& app) {
  FIRApp* ios_app = app.GetPlatformApp();
  auto ios_app_check = FIR_COMPONENT(FIRAppCheckInterop, ios_app.container);
  return absl::make_unique<FirebaseAppCheckCredentialsProvider>(ios_app,
                                                                ios_app_check);
}

}  // namespace firestore
}  // namespace firebase
