/*
 * Copyright 2022 Google LLC
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

#ifndef FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FIRESTORE_LOGGING_UTIL_H_
#define FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FIRESTORE_LOGGING_UTIL_H_

#include "firestore/src/include/firebase/firestore.h"

namespace firebase {
namespace firestore {

// A RAII wrapper that enables Firestore debug logging and then disables it
// upon destruction.
//
// This is useful for enabling Firestore debug logging in a specific test.
//
// Example:
// TEST(MyCoolTest VerifyFirestoreDoesItsThing) {
//   FirestoreDebugLogEnabler firestore_debug_log_enabler;
//   ...
// }
class FirestoreDebugLogEnabler {
 public:
  FirestoreDebugLogEnabler() {
    Firestore::set_log_level(LogLevel::kLogLevelDebug);
  }
  ~FirestoreDebugLogEnabler() {
    Firestore::set_log_level(LogLevel::kLogLevelInfo);
  }
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_UTIL_FIRESTORE_LOGGING_UTIL_H_
