#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_LISTENER_REGISTRATION_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_LISTENER_REGISTRATION_STUB_H_

#include "firestore/src/stub/firestore_stub.h"

namespace firebase {
namespace firestore {

// This is the stub implementation of ListenerRegistration.
class ListenerRegistrationInternal {
 public:
  using ApiType = ListenerRegistration;
  FirestoreInternal* firestore_internal() { return nullptr; }
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_LISTENER_REGISTRATION_STUB_H_
