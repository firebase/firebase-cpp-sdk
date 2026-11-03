/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_

#include <variant>

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/common/event_listener.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {

class FirestoreInternal;

// This is the Android implementation of ListenerRegistration. This is a
// persistent type i.e. all instances are owned by FirestoreInternal.
// ListenerRegistration contains only non-owning pointer to an instance.
//
// We make this non-generic in order to hide the type logic inside.
class ListenerRegistrationInternal {
 public:
  static void Initialize(jni::Loader& loader);

  // Global references will be created from the jobjects. The caller is
  // responsible for cleaning up any local references to jobjects after the
  // constructor returns.
  template <typename T>
  ListenerRegistrationInternal(
      FirestoreInternal* firestore,
      EventListener<T>* event_listener,
      bool owning_event_listener,
      const jni::Object& listener_registration,
      const jni::Object& java_listener = {});

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
  jni::Env GetEnv();

  FirestoreInternal* firestore_ = nullptr;  // not owning
  jni::Global<jni::Object> listener_registration_;
  jni::Global<jni::Object> java_listener_;

  // May own it, see owning_event_listener_. If user pass in an EventListener
  // directly, then the registration does not own it. If user pass in a lambda,
  // then the registration owns the LambdaEventListener that wraps the lambda.
  std::variant<EventListener<DocumentSnapshot>*,
               EventListener<QuerySnapshot>*,
               EventListener<void>*>
      event_listener_ = static_cast<EventListener<void>*>(nullptr);
  bool owning_event_listener_ = false;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_LISTENER_REGISTRATION_ANDROID_H_
