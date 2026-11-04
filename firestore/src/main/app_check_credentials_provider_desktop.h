// Copyright 2023 Google LLC
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

namespace firebase::firestore {

class CppAppCheckCredentialsProvider
    : public firestore::credentials::AppCheckCredentialsProvider {
 public:
  explicit CppAppCheckCredentialsProvider(App& app);
  ~CppAppCheckCredentialsProvider() override;

  CppAppCheckCredentialsProvider(const CppAppCheckCredentialsProvider&) = delete;
  CppAppCheckCredentialsProvider& operator=(const CppAppCheckCredentialsProvider&) = delete;

  void SetCredentialChangeListener(
      firestore::credentials::CredentialChangeListener<std::string> listener) override;
  void GetToken(firestore::credentials::TokenListener<std::string> listener) override;

 private:
  void AddAppCheckStateListener();
  void RemoveAppCheckStateListener();
  static void OnAppCheckStateChanged(const std::string& token, void* context);

  App& app_;
  uint64_t provider_id_;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_APP_CHECK_CREDENTIALS_PROVIDER_DESKTOP_H_
