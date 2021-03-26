#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LISTENER_REGISTRATION_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LISTENER_REGISTRATION_IOS_H_

#include <memory>

#include "firestore/src/ios/firestore_ios.h"
#include "Firestore/core/src/api/listener_registration.h"

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

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LISTENER_REGISTRATION_IOS_H_
