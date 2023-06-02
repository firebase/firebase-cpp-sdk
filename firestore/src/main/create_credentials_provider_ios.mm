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

#include "firestore/src/main/create_credentials_provider.h"

#include <memory>

#import "FIRAppInternal.h"
#import "FIRAuthInterop.h"
#import "FIRComponent.h"
#import "FIRComponentContainer.h"
#import "FIRComponentType.h"
#import "FirebaseCore.h"

#include "Firestore/core/src/credentials/firebase_auth_credentials_provider_apple.h"
#include "app/src/include/firebase/app.h"

namespace firebase {
namespace firestore {

using credentials::AuthCredentialsProvider;
using credentials::FirebaseAuthCredentialsProvider;

std::unique_ptr<AuthCredentialsProvider> CreateCredentialsProvider(App& app) {
  FIRApp* ios_app = app.GetPlatformApp();
  auto ios_auth = FIR_COMPONENT(FIRAuthInterop, ios_app.container);
  return std::make_unique<FirebaseAuthCredentialsProvider>(ios_app, ios_auth);
}

}  // namespace firestore
}  // namespace firebase
