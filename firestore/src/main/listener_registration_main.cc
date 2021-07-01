// Copyright 2021 Google LLC

#include "firestore/src/main/listener_registration_main.h"

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
