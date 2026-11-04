// Copyright 2021 Google LLC
#ifndef FIREBASE_FIRESTORE_SRC_MAIN_CREDENTIALS_PROVIDER_DESKTOP_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_CREDENTIALS_PROVIDER_DESKTOP_H_

#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <string>
#include "Firestore/core/src/credentials/auth_token.h"
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

class FirebaseCppCredentialsProvider final
    : public firestore::credentials::AuthCredentialsProvider {
 public:
  explicit FirebaseCppCredentialsProvider(App& app);
  ~FirebaseCppCredentialsProvider() override;

  FirebaseCppCredentialsProvider(const FirebaseCppCredentialsProvider&) = delete;
  FirebaseCppCredentialsProvider& operator=(const FirebaseCppCredentialsProvider&) = delete;

  void SetCredentialChangeListener(
      firestore::credentials::CredentialChangeListener<firestore::credentials::User> listener)
      override;
  void GetToken(firestore::credentials::TokenListener<firestore::credentials::AuthToken> listener)
      override;
  void InvalidateToken() override;

  struct Contents {
    explicit Contents(App& app) : app(app) {}
    std::recursive_mutex mutex;
    App& app;
    int token_generation = 0;
    firestore::credentials::CredentialChangeListener<firestore::credentials::User> auth_listener;
  };

 private:
  void AddAuthStateListener();
  void RemoveAuthStateListener();
  static void OnAuthStateChanged(void* context);
  void RequestToken(
      firestore::credentials::TokenListener<firestore::credentials::AuthToken> listener);
  bool IsSignedIn() const;

  std::shared_ptr<Contents> contents_;
  void* auth_listener_context_{nullptr};
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_CREDENTIALS_PROVIDER_DESKTOP_H_
