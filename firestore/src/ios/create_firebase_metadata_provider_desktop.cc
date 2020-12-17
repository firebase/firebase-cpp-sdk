#include "firestore/src/ios/create_firebase_metadata_provider.h"
#include "firestore/src/ios/firebase_metadata_provider_desktop.h"
#include "absl/memory/memory.h"

namespace firebase {
namespace firestore {

using remote::FirebaseMetadataProvider;

std::unique_ptr<FirebaseMetadataProvider> CreateFirebaseMetadataProvider(App&) {
  return absl::make_unique<FirebaseMetadataProviderCpp>();
}

}  // namespace firestore
}  // namespace firebase
