#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIREBASE_FIRESTORE_SETTINGS_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIREBASE_FIRESTORE_SETTINGS_IOS_H_

#include "firestore/src/include/firebase/firestore/settings.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/api/settings.h"

namespace firebase {
namespace firestore {

api::Settings ToCoreApi(const Settings& settings);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIREBASE_FIRESTORE_SETTINGS_IOS_H_
