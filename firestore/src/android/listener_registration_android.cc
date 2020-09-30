#include "firestore/src/android/listener_registration_android.h"

#include "app/src/assert.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/include/firebase/firestore/listener_registration.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Object;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/ListenerRegistration";

jni::Method<void> kRemove("remove", "()V");

}  // namespace

void ListenerRegistrationInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kRemove);
}

ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal* firestore,
    EventListener<DocumentSnapshot>* event_listener, bool owning_event_listener,
    const Object& listener_registration)
    : firestore_(firestore),
      listener_registration_(listener_registration),
      document_event_listener_(event_listener),
      owning_event_listener_(owning_event_listener) {
  FIREBASE_ASSERT(firestore != nullptr);
  FIREBASE_ASSERT(event_listener != nullptr);
  FIREBASE_ASSERT(listener_registration);

  firestore->RegisterListenerRegistration(this);
}

ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal* firestore, EventListener<QuerySnapshot>* event_listener,
    bool owning_event_listener, const Object& listener_registration)
    : firestore_(firestore),
      listener_registration_(listener_registration),
      query_event_listener_(event_listener),
      owning_event_listener_(owning_event_listener) {
  FIREBASE_ASSERT(firestore != nullptr);
  FIREBASE_ASSERT(event_listener != nullptr);
  FIREBASE_ASSERT(listener_registration);

  firestore->RegisterListenerRegistration(this);
}

ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal* firestore, EventListener<void>* event_listener,
    bool owning_event_listener, const Object& listener_registration)
    : firestore_(firestore),
      listener_registration_(listener_registration),
      void_event_listener_(event_listener),
      owning_event_listener_(owning_event_listener) {
  FIREBASE_ASSERT(firestore != nullptr);
  FIREBASE_ASSERT(event_listener != nullptr);
  FIREBASE_ASSERT(listener_registration);

  firestore->RegisterListenerRegistration(this);
}

// Destruction only happens when FirestoreInternal de-allocate them.
// FirestoreInternal will hold the lock and unregister all of them. So we do not
// call UnregisterListenerRegistration explicitly here.
ListenerRegistrationInternal::~ListenerRegistrationInternal() {
  if (!listener_registration_) {
    return;
  }

  // Remove listener and release java ListenerRegistration object.
  Env env = GetEnv();
  env.Call(listener_registration_, kRemove);
  listener_registration_.clear();

  // de-allocate owning EventListener object.
  if (owning_event_listener_) {
    delete document_event_listener_;
    delete query_event_listener_;
    delete void_event_listener_;
  }
}

jni::Env ListenerRegistrationInternal::GetEnv() { return firestore_->GetEnv(); }

}  // namespace firestore
}  // namespace firebase
