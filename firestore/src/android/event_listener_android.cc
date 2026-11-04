// Copyright 2020 Google LLC

#include "firestore/src/android/event_listener_android.h"

#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/load_bundle_task_progress_android.h"
#include "firestore/src/android/query_snapshot_android.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;

constexpr char kCppEventListenerClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/CppEventListener";
Method<void> kDiscardPointers("discardPointers", "()V");

constexpr char kDocumentEventListenerClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/DocumentEventListener";
Constructor<Object> kNewDocumentEventListener("(JJ)V");

constexpr char kQueryEventListenerClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/QueryEventListener";
Constructor<Object> kNewQueryEventListener("(JJ)V");

constexpr char kVoidEventListenerClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/VoidEventListener";
Constructor<Object> kNewVoidEventListener("(J)V");

constexpr char kProgressListenerClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/LoadBundleProgressListener";
Constructor<Object> kNewProgressListener("(JJ)V");

template <typename SnapshotT, SnapshotT (FirestoreInternal::*NewFn)(
                                  Env&, const Object&) const>
void OnSnapshotEvent(JNIEnv* raw_env, jlong firestore_ptr, jlong listener_ptr,
                     jobject value, jobject raw_error) {
  if (!firestore_ptr || !listener_ptr) return;
#if FIRESTORE_HAVE_EXCEPTIONS
  try {
#endif
    auto* listener = reinterpret_cast<EventListener<SnapshotT>*>(listener_ptr);
    auto* firestore = reinterpret_cast<FirestoreInternal*>(firestore_ptr);
    Env env(raw_env);
    Object error(raw_error);
    Error error_code = ExceptionInternal::GetErrorCode(env, error);
    std::string error_message = ExceptionInternal::ToString(env, error);
    if (error_code != Error::kErrorOk) {
      listener->OnEvent({}, error_code, error_message);
      return;
    }
    listener->OnEvent((firestore->*NewFn)(env, Object(value)), error_code,
                      error_message);
#if FIRESTORE_HAVE_EXCEPTIONS
  } catch (...) {
  }
#endif
}

}  // namespace

void EventListenerInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kCppEventListenerClassName, kDiscardPointers);

  static const JNINativeMethod kDocNatives[] = {
      {"nativeOnEvent",
       "(JJLjava/lang/Object;Lcom/google/firebase/firestore/"
       "FirebaseFirestoreException;)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::DocumentEventListenerNativeOnEvent)}};
  loader.LoadClass(kDocumentEventListenerClassName, kNewDocumentEventListener);
  loader.RegisterNatives(kDocNatives, FIREBASE_ARRAYSIZE(kDocNatives));

  static const JNINativeMethod kQueryNatives[] = {
      {"nativeOnEvent",
       "(JJLjava/lang/Object;Lcom/google/firebase/firestore/"
       "FirebaseFirestoreException;)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::QueryEventListenerNativeOnEvent)}};
  loader.LoadClass(kQueryEventListenerClassName, kNewQueryEventListener);
  loader.RegisterNatives(kQueryNatives, FIREBASE_ARRAYSIZE(kQueryNatives));

  static const JNINativeMethod kVoidNatives[] = {
      {"nativeOnEvent", "(J)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::VoidEventListenerNativeOnEvent)}};
  loader.LoadClass(kVoidEventListenerClassName, kNewVoidEventListener);
  loader.RegisterNatives(kVoidNatives, FIREBASE_ARRAYSIZE(kVoidNatives));

  static const JNINativeMethod kProgressNatives[] = {
      {"nativeOnProgress", "(JJLjava/lang/Object;)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::ProgressListenerNativeOnProgress)}};
  loader.LoadClass(kProgressListenerClassName, kNewProgressListener);
  loader.RegisterNatives(kProgressNatives,
                         FIREBASE_ARRAYSIZE(kProgressNatives));
}

void EventListenerInternal::DocumentEventListenerNativeOnEvent(
    JNIEnv* raw_env, jclass, jlong firestore_ptr, jlong listener_ptr,
    jobject value, jobject raw_error) {
  OnSnapshotEvent<DocumentSnapshot, &FirestoreInternal::NewDocumentSnapshot>(
      raw_env, firestore_ptr, listener_ptr, value, raw_error);
}

void EventListenerInternal::QueryEventListenerNativeOnEvent(
    JNIEnv* raw_env, jclass, jlong firestore_ptr, jlong listener_ptr,
    jobject value, jobject raw_error) {
  OnSnapshotEvent<QuerySnapshot, &FirestoreInternal::NewQuerySnapshot>(
      raw_env, firestore_ptr, listener_ptr, value, raw_error);
}

void EventListenerInternal::VoidEventListenerNativeOnEvent(JNIEnv*, jclass,
                                                           jlong listener_ptr) {
  if (!listener_ptr) return;
#if FIRESTORE_HAVE_EXCEPTIONS
  try {
#endif
    reinterpret_cast<EventListener<void>*>(listener_ptr)
        ->OnEvent(Error::kErrorOk, EmptyString());
#if FIRESTORE_HAVE_EXCEPTIONS
  } catch (...) {
  }
#endif
}

void EventListenerInternal::ProgressListenerNativeOnProgress(
    JNIEnv* raw_env, jclass, jlong firestore_ptr, jlong listener_ptr,
    jobject progress) {
  if (!firestore_ptr || !listener_ptr || !progress) return;
#if FIRESTORE_HAVE_EXCEPTIONS
  try {
#endif
    auto* listener =
        reinterpret_cast<EventListener<LoadBundleTaskProgress>*>(listener_ptr);
    Env env(raw_env);
    LoadBundleTaskProgress cpp_progress =
        LoadBundleTaskProgressInternal::ToPublic(env, Object(progress));
    if (env.ok()) {
      listener->OnEvent(cpp_progress, Error::kErrorOk, EmptyString());
    }
#if FIRESTORE_HAVE_EXCEPTIONS
  } catch (...) {
  }
#endif
}

Local<Object> EventListenerInternal::Create(
    Env& env, FirestoreInternal* firestore,
    EventListener<DocumentSnapshot>* listener) {
  return env.New(kNewDocumentEventListener, reinterpret_cast<jlong>(firestore),
                 reinterpret_cast<jlong>(listener));
}

Local<Object> EventListenerInternal::Create(
    Env& env, FirestoreInternal* firestore,
    EventListener<QuerySnapshot>* listener) {
  return env.New(kNewQueryEventListener, reinterpret_cast<jlong>(firestore),
                 reinterpret_cast<jlong>(listener));
}

Local<Object> EventListenerInternal::Create(Env& env,
                                            EventListener<void>* listener) {
  return env.New(kNewVoidEventListener, reinterpret_cast<jlong>(listener));
}

Local<Object> EventListenerInternal::Create(
    Env& env, FirestoreInternal* firestore,
    EventListener<LoadBundleTaskProgress>* listener) {
  return env.New(kNewProgressListener, reinterpret_cast<jlong>(firestore),
                 reinterpret_cast<jlong>(listener));
}

void EventListenerInternal::DiscardPointers(Env& env, const Object& listener) {
  if (listener) env.Call(listener, kDiscardPointers);
}

}  // namespace firebase::firestore
