// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_CREATE_FIREBASE_METADATA_PROVIDER_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_CREATE_FIREBASE_METADATA_PROVIDER_H_

#include <memory>

#include "Firestore/core/src/remote/firebase_metadata_provider.h"

namespace firebase {

class App;

namespace firestore {

std::unique_ptr<remote::FirebaseMetadataProvider>
CreateFirebaseMetadataProvider(App& app);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_CREATE_FIREBASE_METADATA_PROVIDER_H_
