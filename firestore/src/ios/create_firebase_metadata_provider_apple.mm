#include "firestore/src/ios/create_firebase_metadata_provider.h"

#include "app/src/include/firebase/app.h"
#include "Firestore/core/src/remote/firebase_metadata_provider_apple.h"
#include "absl/memory/memory.h"

namespace firebase {
namespace firestore {

using remote::FirebaseMetadataProvider;
using remote::FirebaseMetadataProviderApple;

std::unique_ptr<FirebaseMetadataProvider> CreateFirebaseMetadataProvider(App& app) {
  return absl::make_unique<FirebaseMetadataProviderApple>(app.GetPlatformApp());
}

}  // namespace firestore
}  // namespace firebase
