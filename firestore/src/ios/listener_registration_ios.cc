#include "firestore/src/ios/listener_registration_ios.h"

#include <utility>

#include "app/src/assert.h"

namespace firebase {
namespace firestore {

ListenerRegistrationInternal::ListenerRegistrationInternal(
    std::unique_ptr<api::ListenerRegistration> registration,
    FirestoreInternal* firestore)
    : registration_{std::move(registration)}, firestore_{firestore} {
  firestore->RegisterListenerRegistration(this);
}

ListenerRegistrationInternal::~ListenerRegistrationInternal() { Remove(); }

}  // namespace firestore
}  // namespace firebase
