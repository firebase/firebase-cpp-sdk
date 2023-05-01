/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_REMOTE_CONFIG_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_
#define FIREBASE_REMOTE_CONFIG_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/common/event_listener.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace remote_config {

// This is the Android implementation of ConfigUpdateListenerRegistration. This
// is a persistent type i.e. all instances are owned by RemoteConfigInternal.
// ConfigUpdateListenerRegistration contains only no-owning pointer to an instance.
//
// We make this non-generic in order to hide the type logic inside.
class ListenerRegistrationInternal {
 public:
  // Takes a Global reference to a native ConfigUpdateListenerRegistration.
  // This global ref will be destroyed when this object is
  ListenerRegistrationInternal(jobject listener_registration);

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

  jni::Env GetEnv();

  FirestoreInternal* firestore_ = nullptr;  // not owning
  jni::Global<jni::Object> listener_registration_;

  // May own it, see owning_event_listener_. If user pass in an EventListener
  // directly, then the registration does not own it. If user pass in a lambda,
  // then the registration owns the LambdaEventListener that wraps the lambda.
  EventListener<DocumentSnapshot>* document_event_listener_ = nullptr;
  EventListener<QuerySnapshot>* query_event_listener_ = nullptr;
  EventListener<void>* void_event_listener_ = nullptr;
  bool owning_event_listener_;
};

}  // namespace remote_config
}  // namespace firebase
#endif  // FIREBASE_REMOTE_CONFIG_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_
