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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/common/event_listener.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class EventListenerInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static void DocumentEventListenerNativeOnEvent(JNIEnv* env,
                                                 jclass clazz,
                                                 jlong firestore_ptr,
                                                 jlong listener_ptr,
                                                 jobject value,
                                                 jobject error);
  static void QueryEventListenerNativeOnEvent(JNIEnv* env,
                                              jclass clazz,
                                              jlong firestore_ptr,
                                              jlong listener_ptr,
                                              jobject value,
                                              jobject error);
  static void VoidEventListenerNativeOnEvent(JNIEnv* env,
                                             jclass clazz,
                                             jlong listener_ptr);
  static void ProgressListenerNativeOnProgress(JNIEnv* env,
                                               jclass clazz,
                                               jlong firestore_ptr,
                                               jlong listener_ptr,
                                               jobject progress);

  static jni::Local<jni::Object> Create(
      jni::Env& env,
      FirestoreInternal* firestore,
      EventListener<DocumentSnapshot>* listener);

  static jobject EventListenerToJavaEventListener(
      JNIEnv* env,
      FirestoreInternal* firestore,
      EventListener<DocumentSnapshot>* listener);

  static jni::Local<jni::Object> Create(jni::Env& env,
                                        FirestoreInternal* firestore,
                                        EventListener<QuerySnapshot>* listener);

  static jobject EventListenerToJavaEventListener(
      JNIEnv* env,
      FirestoreInternal* firestore,
      EventListener<QuerySnapshot>* listener);

  static jni::Local<jni::Object> Create(jni::Env& env,
                                        EventListener<void>* listener);

  static jobject EventListenerToJavaRunnable(JNIEnv* env,
                                             EventListener<void>* listener);

  static jni::Local<jni::Object> Create(
      jni::Env& env,
      FirestoreInternal* firestore,
      EventListener<LoadBundleTaskProgress>* listener);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_
