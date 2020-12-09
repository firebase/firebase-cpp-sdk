#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CREATE_FIREBASE_METADATA_PROVIDER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CREATE_FIREBASE_METADATA_PROVIDER_H_

#include <memory>

#include "Firestore/core/src/remote/firebase_metadata_provider.h"

namespace firebase {

class App;

namespace firestore {

std::unique_ptr<remote::FirebaseMetadataProvider>
CreateFirebaseMetadataProvider(App& app);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CREATE_FIREBASE_METADATA_PROVIDER_H_
