// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_FIREBASE_METADATA_PROVIDER_DESKTOP_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_FIREBASE_METADATA_PROVIDER_DESKTOP_H_

#include "Firestore/core/src/remote/firebase_metadata_provider.h"

namespace firebase {

namespace firestore {

class FirebaseMetadataProviderCpp : public remote::FirebaseMetadataProvider {
 public:
  void UpdateMetadata(grpc::ClientContext& context) override;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_FIREBASE_METADATA_PROVIDER_DESKTOP_H_
