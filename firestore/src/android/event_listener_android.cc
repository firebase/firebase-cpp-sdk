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

#include "firestore/src/android/event_listener_android.h"

#include <utility>

#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/load_bundle_task_progress_android.h"
#include "firestore/src/android/query_snapshot_android.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;

constexpr char kCppEventListenerClassName[] =
    "com/google/firebase/firestore/internal/cpp/CppEventListener";
Method<void> kDiscardPointers("discardPointers", "()V");

constexpr char kDocumentEventListenerClassName[] =
    "com/google/firebase/firestore/internal/cpp/DocumentEventListener";
Constructor<Object> kNewDocumentEventListener("(JJ)V");

constexpr char kQueryEventListenerClassName[] =
    "com/google/firebase/firestore/internal/cpp/QueryEventListener";
Constructor<Object> kNewQueryEventListener("(JJ)V");

constexpr char kVoidEventListenerClassName[] =
    "com/google/firebase/firestore/internal/cpp/VoidEventListener";
Constructor<Object> kNewVoidEventListener("(J)V");

constexpr char kProgressListenerClassName[] =
    "com/google/firebase/firestore/internal/cpp/LoadBundleProgressListener";
Constructor<Object> kNewProgressListener("(JJ)V");

}  // namespace

/* static */
void EventListenerInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kCppEventListenerClassName, kDiscardPointers);

  static const JNINativeMethod kDocumentEventListenerNatives[] = {
      {"nativeOnEvent",
       "(JJLjava/lang/Object;Lcom/google/firebase/firestore/"
       "FirebaseFirestoreException;)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::DocumentEventListenerNativeOnEvent)}};
  loader.LoadClass(kDocumentEventListenerClassName, kNewDocumentEventListener);
  loader.RegisterNatives(kDocumentEventListenerNatives,
                         FIREBASE_ARRAYSIZE(kDocumentEventListenerNatives));

  static const JNINativeMethod kQueryEventListenerNatives[] = {
      {"nativeOnEvent",
       "(JJLjava/lang/Object;Lcom/google/firebase/firestore/"
       "FirebaseFirestoreException;)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::QueryEventListenerNativeOnEvent)}};
  loader.LoadClass(kQueryEventListenerClassName, kNewQueryEventListener);
  loader.RegisterNatives(kQueryEventListenerNatives,
                         FIREBASE_ARRAYSIZE(kQueryEventListenerNatives));

  static const JNINativeMethod kVoidEventListenerNatives[] = {
      {"nativeOnEvent", "(J)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::VoidEventListenerNativeOnEvent)}};
  loader.LoadClass(kVoidEventListenerClassName, kNewVoidEventListener);
  loader.RegisterNatives(kVoidEventListenerNatives,
                         FIREBASE_ARRAYSIZE(kVoidEventListenerNatives));

  static const JNINativeMethod kProgressListenerNatives[] = {
      {"nativeOnProgress", "(JJLjava/lang/Object;)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::ProgressListenerNativeOnProgress)}};
  loader.LoadClass(kProgressListenerClassName, kNewProgressListener);
  loader.RegisterNatives(kProgressListenerNatives,
                         FIREBASE_ARRAYSIZE(kProgressListenerNatives));
}

void EventListenerInternal::DocumentEventListenerNativeOnEvent(
    JNIEnv* raw_env,
    jclass,
    jlong firestore_ptr,
    jlong listener_ptr,
    jobject value,
    jobject raw_error) {
  if (firestore_ptr == 0 || listener_ptr == 0) {
    return;
  }
  auto* listener =
      reinterpret_cast<EventListener<DocumentSnapshot>*>(listener_ptr);
  auto* firestore = reinterpret_cast<FirestoreInternal*>(firestore_ptr);

  Env env(raw_env);
  Object error(raw_error);
  Error error_code = ExceptionInternal::GetErrorCode(env, error);
  std::string error_message = ExceptionInternal::ToString(env, error);
  if (error_code != Error::kErrorOk) {
    listener->OnEvent({}, error_code, error_message);
    return;
  }

  auto snapshot = firestore->NewDocumentSnapshot(env, Object(value));
  listener->OnEvent(snapshot, error_code, error_message);
}

/* static */
void EventListenerInternal::QueryEventListenerNativeOnEvent(JNIEnv* raw_env,
                                                            jclass,
                                                            jlong firestore_ptr,
                                                            jlong listener_ptr,
                                                            jobject value,
                                                            jobject raw_error) {
  if (firestore_ptr == 0 || listener_ptr == 0) {
    return;
  }
  auto* listener =
      reinterpret_cast<EventListener<QuerySnapshot>*>(listener_ptr);
  auto* firestore = reinterpret_cast<FirestoreInternal*>(firestore_ptr);

  Env env(raw_env);
  Object error(raw_error);
  Error error_code = ExceptionInternal::GetErrorCode(env, error);
  std::string error_message = ExceptionInternal::ToString(env, error);
  if (error_code != Error::kErrorOk) {
    listener->OnEvent({}, error_code, error_message);
    return;
  }

  auto snapshot = firestore->NewQuerySnapshot(env, Object(value));
  listener->OnEvent(snapshot, error_code, error_message);
}

/* static */
void EventListenerInternal::VoidEventListenerNativeOnEvent(JNIEnv*,
                                                           jclass,
                                                           jlong listener_ptr) {
  if (listener_ptr == 0) {
    return;
  }
  auto* listener = reinterpret_cast<EventListener<void>*>(listener_ptr);
  listener->OnEvent(Error::kErrorOk, EmptyString());
}

/* static */
void EventListenerInternal::ProgressListenerNativeOnProgress(
    JNIEnv*,
    jclass,
    jlong firestore_ptr,
    jlong listener_ptr,
    jobject progress) {
  if (listener_ptr == 0) {
    return;
  }
  auto* firestore = reinterpret_cast<FirestoreInternal*>(firestore_ptr);
  auto* listener =
      reinterpret_cast<EventListener<LoadBundleTaskProgress>*>(listener_ptr);
  LoadBundleTaskProgressInternal internal(firestore, Object(progress));
  LoadBundleTaskProgress cpp_progress(
      internal.documents_loaded(), internal.total_documents(),
      internal.bytes_loaded(), internal.total_bytes(), internal.state());
  listener->OnEvent(cpp_progress, Error::kErrorOk, EmptyString());
}

Local<Object> EventListenerInternal::Create(
    Env& env,
    FirestoreInternal* firestore,
    EventListener<DocumentSnapshot>* listener) {
  return env.New(kNewDocumentEventListener, reinterpret_cast<jlong>(firestore),
                 reinterpret_cast<jlong>(listener));
}

Local<Object> EventListenerInternal::Create(
    Env& env,
    FirestoreInternal* firestore,
    EventListener<QuerySnapshot>* listener) {
  return env.New(kNewQueryEventListener, reinterpret_cast<jlong>(firestore),
                 reinterpret_cast<jlong>(listener));
}

Local<Object> EventListenerInternal::Create(Env& env,
                                            EventListener<void>* listener) {
  return env.New(kNewVoidEventListener, reinterpret_cast<jlong>(listener));
}

Local<Object> EventListenerInternal::Create(
    Env& env,
    FirestoreInternal* firestore,
    EventListener<LoadBundleTaskProgress>* listener) {
  return env.New(kNewProgressListener, reinterpret_cast<jlong>(firestore),
                 reinterpret_cast<jlong>(listener));
}

}  // namespace firestore
}  // namespace firebase
