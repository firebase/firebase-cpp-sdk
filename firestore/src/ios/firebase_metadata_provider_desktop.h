#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIREBASE_METADATA_PROVIDER_DESKTOP_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIREBASE_METADATA_PROVIDER_DESKTOP_H_

#include "Firestore/core/src/remote/firebase_metadata_provider.h"

namespace firebase {

namespace firestore {

class FirebaseMetadataProviderCpp : public remote::FirebaseMetadataProvider {
 public:
  void UpdateMetadata(grpc::ClientContext& context) override;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIREBASE_METADATA_PROVIDER_DESKTOP_H_
