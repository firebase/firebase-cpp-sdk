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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_CREATE_FIREBASE_METADATA_PROVIDER_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_CREATE_FIREBASE_METADATA_PROVIDER_H_

#include <memory>

#include "Firestore/core/src/remote/firebase_metadata_provider.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {

class App;

namespace firestore {

std::unique_ptr<remote::FirebaseMetadataProvider>
CreateFirebaseMetadataProvider(App& app);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_CREATE_FIREBASE_METADATA_PROVIDER_H_
