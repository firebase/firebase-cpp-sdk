#include "firestore/src/android/listener_registration_android.h"

#include "app/src/assert.h"
#include "app/src/util_android.h"
#include "firestore/src/include/firebase/firestore/listener_registration.h"

namespace firebase {
namespace firestore {

#define LISTENER_REGISTRATION_METHODS(X) X(Remove, "remove", "()V")
METHOD_LOOKUP_DECLARATION(listener_registration, LISTENER_REGISTRATION_METHODS)
METHOD_LOOKUP_DEFINITION(listener_registration,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/ListenerRegistration",
                         LISTENER_REGISTRATION_METHODS)

ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal* firestore,
    EventListener<DocumentSnapshot>* event_listener, bool owning_event_listener,
    jobject listener_registration)
    : firestore_(firestore),
      listener_registration_(
          firestore->app()->GetJNIEnv()->NewGlobalRef(listener_registration)),
      document_event_listener_(event_listener),
      owning_event_listener_(owning_event_listener) {
  FIREBASE_ASSERT(firestore != nullptr);
  FIREBASE_ASSERT(event_listener != nullptr);
  FIREBASE_ASSERT(listener_registration != nullptr);

  firestore->RegisterListenerRegistration(this);
}

ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal* firestore, EventListener<QuerySnapshot>* event_listener,
    bool owning_event_listener, jobject listener_registration)
    : firestore_(firestore),
      listener_registration_(
          firestore->app()->GetJNIEnv()->NewGlobalRef(listener_registration)),
      query_event_listener_(event_listener),
      owning_event_listener_(owning_event_listener) {
  FIREBASE_ASSERT(firestore != nullptr);
  FIREBASE_ASSERT(event_listener != nullptr);
  FIREBASE_ASSERT(listener_registration != nullptr);

  firestore->RegisterListenerRegistration(this);
}

ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal* firestore, EventListener<void>* event_listener,
    bool owning_event_listener, jobject listener_registration)
    : firestore_(firestore),
      listener_registration_(
          firestore->app()->GetJNIEnv()->NewGlobalRef(listener_registration)),
      void_event_listener_(event_listener),
      owning_event_listener_(owning_event_listener) {
  FIREBASE_ASSERT(firestore != nullptr);
  FIREBASE_ASSERT(event_listener != nullptr);
  FIREBASE_ASSERT(listener_registration != nullptr);

  firestore->RegisterListenerRegistration(this);
}

// Destruction only happens when FirestoreInternal de-allocate them.
// FirestoreInternal will hold the lock and unregister all of them. So we do not
// call UnregisterListenerRegistration explicitly here.
ListenerRegistrationInternal::~ListenerRegistrationInternal() {
  if (listener_registration_ == nullptr) {
    return;
  }

  // Remove listener and release java ListenerRegistration object.
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  env->CallVoidMethod(
      listener_registration_,
      listener_registration::GetMethodId(listener_registration::kRemove));
  env->DeleteGlobalRef(listener_registration_);
  util::CheckAndClearJniExceptions(env);
  listener_registration_ = nullptr;

  // de-allocate owning EventListener object.
  if (owning_event_listener_) {
    delete document_event_listener_;
    delete query_event_listener_;
    delete void_event_listener_;
  }
}

/* static */
bool ListenerRegistrationInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = listener_registration::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void ListenerRegistrationInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  listener_registration::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
