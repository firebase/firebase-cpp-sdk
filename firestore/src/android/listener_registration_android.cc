// Copyright 2020 Google LLC

#include "firestore/src/android/listener_registration_android.h"

#include "app/src/assert.h"
#include "firestore/src/android/event_listener_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/include/firebase/firestore/listener_registration.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore {
namespace {

using jni::Env;
using jni::ExceptionClearGuard;
using jni::Object;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/ListenerRegistration";
jni::Method<void> kRemove("remove", "()V");

}  // namespace

void ListenerRegistrationInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kRemove);
}

template <typename T>
ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal* firestore, EventListener<T>* event_listener,
    bool owning_event_listener, const Object& listener_registration,
    const Object& java_listener)
    : firestore_(firestore),
      listener_registration_(listener_registration),
      java_listener_(java_listener),
      event_listener_(event_listener),
      owning_event_listener_(owning_event_listener) {
  FIREBASE_ASSERT(firestore && event_listener && listener_registration);
  firestore->RegisterListenerRegistration(this);
}

template ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal*, EventListener<DocumentSnapshot>*, bool, const Object&,
    const Object&);
template ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal*, EventListener<QuerySnapshot>*, bool, const Object&,
    const Object&);
template ListenerRegistrationInternal::ListenerRegistrationInternal(
    FirestoreInternal*, EventListener<void>*, bool, const Object&,
    const Object&);

ListenerRegistrationInternal::~ListenerRegistrationInternal() {
  Env env = GetEnv();
  ExceptionClearGuard exception_clear_guard(env);
  if (listener_registration_) {
    env.Call(listener_registration_, kRemove);
    if (!env.ok()) env.ExceptionClear();
    listener_registration_.clear();
  }
  if (java_listener_) {
    EventListenerInternal::DiscardPointers(env, java_listener_);
    java_listener_.clear();
  }
  if (owning_event_listener_) {
    std::visit([](auto* listener) { delete listener; }, event_listener_);
  }
}

jni::Env ListenerRegistrationInternal::GetEnv() { return firestore_->GetEnv(); }

}  // namespace firebase::firestore
