#include "firestore/src/android/event_listener_android.h"

#include <utility>

#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/query_snapshot_android.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firebase/firestore/firestore_errors.h"

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
}

void EventListenerInternal::DocumentEventListenerNativeOnEvent(
    JNIEnv* raw_env, jclass, jlong firestore_ptr, jlong listener_ptr,
    jobject value, jobject raw_error) {
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
void EventListenerInternal::QueryEventListenerNativeOnEvent(
    JNIEnv* raw_env, jclass, jlong firestore_ptr, jlong listener_ptr,
    jobject value, jobject raw_error) {
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
void EventListenerInternal::VoidEventListenerNativeOnEvent(JNIEnv*, jclass,
                                                           jlong listener_ptr) {
  if (listener_ptr == 0) {
    return;
  }
  auto* listener = reinterpret_cast<EventListener<void>*>(listener_ptr);
  listener->OnEvent(Error::kErrorOk, EmptyString());
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

}  // namespace firestore
}  // namespace firebase
