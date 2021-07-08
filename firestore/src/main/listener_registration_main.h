// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_LISTENER_REGISTRATION_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_LISTENER_REGISTRATION_MAIN_H_

#include <memory>

#include "Firestore/core/src/api/listener_registration.h"
#include "firestore/src/main/firestore_main.h"

namespace firebase {
namespace firestore {

class ListenerRegistrationInternal {
 public:
  ListenerRegistrationInternal(
      std::unique_ptr<api::ListenerRegistration> registration,
      FirestoreInternal* firestore);
  ~ListenerRegistrationInternal();

  ListenerRegistrationInternal(const ListenerRegistrationInternal&) = delete;
  ListenerRegistrationInternal& operator=(const ListenerRegistrationInternal&) =
      delete;

  FirestoreInternal* firestore_internal() { return firestore_; }

 private:
  friend class FirestoreInternal;

  void Remove() { registration_->Remove(); }

  std::unique_ptr<api::ListenerRegistration> registration_;
  FirestoreInternal* firestore_ = nullptr;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_LISTENER_REGISTRATION_MAIN_H_
