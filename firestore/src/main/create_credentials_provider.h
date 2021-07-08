// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_CREATE_CREDENTIALS_PROVIDER_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_CREATE_CREDENTIALS_PROVIDER_H_

#include <memory>

#include "Firestore/core/src/auth/credentials_provider.h"

namespace firebase {

class App;

namespace firestore {

std::unique_ptr<auth::CredentialsProvider> CreateCredentialsProvider(App& app);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_CREATE_CREDENTIALS_PROVIDER_H_
