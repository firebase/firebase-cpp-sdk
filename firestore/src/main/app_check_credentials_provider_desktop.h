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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_APP_CHECK_CREDENTIALS_PROVIDER_DESKTOP_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_APP_CHECK_CREDENTIALS_PROVIDER_DESKTOP_H_

#include <string>

#include "Firestore/core/src/credentials/credentials_fwd.h"
#include "Firestore/core/src/credentials/credentials_provider.h"
#include "Firestore/core/src/credentials/user.h"
#include "Firestore/core/src/util/statusor.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

// Glues together C++ Firebase App Check and Firestore: allows Firestore to
// listen to App Check events and to retrieve App Check tokens. Thread-safe.
//
// This is a language-specific implementation of `AppCheckCredentialsProvider`
// that works with the public C++ App Check.
class CppAppCheckCredentialsProvider
    : public firestore::credentials::AppCheckCredentialsProvider {
 public:
  explicit CppAppCheckCredentialsProvider(App& app);
  ~CppAppCheckCredentialsProvider() override;

  CppAppCheckCredentialsProvider(const CppAppCheckCredentialsProvider&) =
      delete;
  CppAppCheckCredentialsProvider& operator=(
      const CppAppCheckCredentialsProvider&) = delete;

  // `firestore::credentials::AppCheckCredentialsProvider` interface.
  void SetCredentialChangeListener(
      firestore::credentials::CredentialChangeListener<std::string> listener)
      override;
  void GetToken(
      firestore::credentials::TokenListener<std::string> listener) override;

 private:
  void AddAppCheckStateListener();
  void RemoveAppCheckStateListener();

  // Callback for the function registry-based pseudo-AuthStateListener
  // interface.
  static void OnAppCheckStateChanged(const std::string& token, void* context);

  App& app_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_APP_CHECK_CREDENTIALS_PROVIDER_DESKTOP_H_
