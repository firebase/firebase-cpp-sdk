#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_

#include <jni.h>
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/event_listener.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"

namespace firebase {
namespace firestore {

class FirestoreInternal;
class ListenerRegistration;

// This is the Android implementation of ListenerRegistration. This is a
// persistent type i.e. all instances are owned by FirestoreInternal.
// ListenerRegistration contains only no-owning pointer to an instance.
//
// We make this non-generic in order to hide the type logic inside.
class ListenerRegistrationInternal {
 public:
  using ApiType = ListenerRegistration;

  // Global references will be created from the jobjects. The caller is
  // responsible for cleaning up any local references to jobjects after the
  // constructor returns.
  ListenerRegistrationInternal(FirestoreInternal* firestore,
                               EventListener<DocumentSnapshot>* event_listener,
                               bool owning_event_listener,
                               jobject listener_registration);
  ListenerRegistrationInternal(FirestoreInternal* firestore,
                               EventListener<QuerySnapshot>* event_listener,
                               bool owning_event_listener,
                               jobject listener_registration);
  ListenerRegistrationInternal(FirestoreInternal* firestore,
                               EventListener<void>* event_listener,
                               bool owning_event_listener,
                               jobject listener_registration);

  // Delete the default one to make the ownership more obvious i.e.
  // FirestoreInternal owns each instance and forbid anyone else to make copy.
  ListenerRegistrationInternal(const ListenerRegistrationInternal& another) =
      delete;
  ListenerRegistrationInternal(ListenerRegistrationInternal&& another) = delete;

  ~ListenerRegistrationInternal();

  // So far, there is no use of assignment. So we do not bother to define our
  // own and delete the default one, which does not copy jobject properly.
  ListenerRegistrationInternal& operator=(
      const ListenerRegistrationInternal& another) = delete;
  ListenerRegistrationInternal& operator=(
      ListenerRegistrationInternal&& another) = delete;

  FirestoreInternal* firestore_internal() { return firestore_; }

 private:
  friend class DocumentReferenceInternal;
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);

  FirestoreInternal* firestore_ = nullptr;  // not owning
  jobject listener_registration_ = nullptr;

  // May own it, see owning_event_listener_. If user pass in an EventListener
  // directly, then the registration does not own it. If user pass in a lambda,
  // then the registration owns the LambdaEventListener that wraps the lambda.
  EventListener<DocumentSnapshot>* document_event_listener_ = nullptr;
  EventListener<QuerySnapshot>* query_event_listener_ = nullptr;
  EventListener<void>* void_event_listener_ = nullptr;
  bool owning_event_listener_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_
