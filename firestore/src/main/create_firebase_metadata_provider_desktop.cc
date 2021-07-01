// Copyright 2021 Google LLC

#include "absl/memory/memory.h"
#include "firestore/src/main/create_firebase_metadata_provider.h"
#include "firestore/src/main/firebase_metadata_provider_desktop.h"

namespace firebase {
namespace firestore {

using remote::FirebaseMetadataProvider;

std::unique_ptr<FirebaseMetadataProvider> CreateFirebaseMetadataProvider(App&) {
  return absl::make_unique<FirebaseMetadataProviderCpp>();
}

}  // namespace firestore
}  // namespace firebase
