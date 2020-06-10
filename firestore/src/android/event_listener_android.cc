#include "firestore/src/android/event_listener_android.h"

#include <utility>

#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/firebase_firestore_exception_android.h"
#include "firestore/src/android/query_snapshot_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {

#define CPP_EVENT_LISTENER_METHODS(X) \
  X(DiscardPointers, "discardPointers", "()V")
METHOD_LOOKUP_DECLARATION(cpp_event_listener, CPP_EVENT_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    cpp_event_listener,
    "com/google/firebase/firestore/internal/cpp/CppEventListener",
    CPP_EVENT_LISTENER_METHODS)

#define DOCUMENT_EVENT_LISTENER_METHODS(X) X(Constructor, "<init>", "(JJ)V")
METHOD_LOOKUP_DECLARATION(document_event_listener,
                          DOCUMENT_EVENT_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    document_event_listener,
    "com/google/firebase/firestore/internal/cpp/DocumentEventListener",
    DOCUMENT_EVENT_LISTENER_METHODS)

#define QUERY_EVENT_LISTENER_METHODS(X) X(Constructor, "<init>", "(JJ)V")
METHOD_LOOKUP_DECLARATION(query_event_listener, QUERY_EVENT_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    query_event_listener,
    "com/google/firebase/firestore/internal/cpp/QueryEventListener",
    QUERY_EVENT_LISTENER_METHODS)

#define VOID_EVENT_LISTENER_METHODS(X) X(Constructor, "<init>", "(J)V")
METHOD_LOOKUP_DECLARATION(void_event_listener, VOID_EVENT_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    void_event_listener,
    "com/google/firebase/firestore/internal/cpp/VoidEventListener",
    VOID_EVENT_LISTENER_METHODS)

/* static */
void EventListenerInternal::DocumentEventListenerNativeOnEvent(
    JNIEnv* env, jclass clazz, jlong firestore_ptr, jlong listener_ptr,
    jobject value, jobject error) {
  if (firestore_ptr == 0 || listener_ptr == 0) {
    return;
  }
  EventListener<DocumentSnapshot>* listener =
      reinterpret_cast<EventListener<DocumentSnapshot>*>(listener_ptr);
  Error error_code =
      FirebaseFirestoreExceptionInternal::ToErrorCode(env, error);
  if (error_code != Error::kErrorOk) {
    listener->OnEvent(DocumentSnapshot{}, error_code);
    return;
  }

  FirestoreInternal* firestore =
      reinterpret_cast<FirestoreInternal*>(firestore_ptr);
  DocumentSnapshot snapshot(new DocumentSnapshotInternal{firestore, value});
  listener->OnEvent(snapshot, error_code);
}

/* static */
void EventListenerInternal::QueryEventListenerNativeOnEvent(
    JNIEnv* env, jclass clazz, jlong firestore_ptr, jlong listener_ptr,
    jobject value, jobject error) {
  if (firestore_ptr == 0 || listener_ptr == 0) {
    return;
  }
  EventListener<QuerySnapshot>* listener =
      reinterpret_cast<EventListener<QuerySnapshot>*>(listener_ptr);
  Error error_code =
      FirebaseFirestoreExceptionInternal::ToErrorCode(env, error);
  if (error_code != Error::kErrorOk) {
    listener->OnEvent(QuerySnapshot{}, error_code);
    return;
  }

  FirestoreInternal* firestore =
      reinterpret_cast<FirestoreInternal*>(firestore_ptr);
  QuerySnapshot snapshot(new QuerySnapshotInternal{firestore, value});
  listener->OnEvent(snapshot, error_code);
}

/* static */
void EventListenerInternal::VoidEventListenerNativeOnEvent(JNIEnv* env,
                                                           jclass clazz,
                                                           jlong listener_ptr) {
  if (listener_ptr == 0) {
    return;
  }
  EventListener<void>* listener =
      reinterpret_cast<EventListener<void>*>(listener_ptr);

  listener->OnEvent(Error::kErrorOk);
}

/* static */
jobject EventListenerInternal::EventListenerToJavaEventListener(
    JNIEnv* env, FirestoreInternal* firestore,
    EventListener<DocumentSnapshot>* listener) {
  jobject result = env->NewObject(document_event_listener::GetClass(),
                                  document_event_listener::GetMethodId(
                                      document_event_listener::kConstructor),
                                  reinterpret_cast<jlong>(firestore),
                                  reinterpret_cast<jlong>(listener));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
jobject EventListenerInternal::EventListenerToJavaEventListener(
    JNIEnv* env, FirestoreInternal* firestore,
    EventListener<QuerySnapshot>* listener) {
  jobject result = env->NewObject(
      query_event_listener::GetClass(),
      query_event_listener::GetMethodId(query_event_listener::kConstructor),
      reinterpret_cast<jlong>(firestore), reinterpret_cast<jlong>(listener));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
jobject EventListenerInternal::EventListenerToJavaRunnable(
    JNIEnv* env, EventListener<void>* listener) {
  jobject result = env->NewObject(
      void_event_listener::GetClass(),
      void_event_listener::GetMethodId(void_event_listener::kConstructor),
      reinterpret_cast<jlong>(listener));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
bool EventListenerInternal::InitializeEmbeddedClasses(
    App* app, const std::vector<internal::EmbeddedFile>* embedded_files) {
  static const JNINativeMethod kDocumentEventListenerNatives[] = {
      {"nativeOnEvent",
       "(JJLjava/lang/Object;Lcom/google/firebase/firestore/"
       "FirebaseFirestoreException;)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::DocumentEventListenerNativeOnEvent)}};
  static const JNINativeMethod kQueryEventListenerNatives[] = {
      {"nativeOnEvent",
       "(JJLjava/lang/Object;Lcom/google/firebase/firestore/"
       "FirebaseFirestoreException;)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::QueryEventListenerNativeOnEvent)}};
  static const JNINativeMethod kVoidEventListenerNatives[] = {
      {"nativeOnEvent", "(J)V",
       reinterpret_cast<void*>(
           &EventListenerInternal::VoidEventListenerNativeOnEvent)}};

  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result =
      // Cache classes
      cpp_event_listener::CacheClassFromFiles(env, activity, embedded_files) &&
      document_event_listener::CacheClassFromFiles(env, activity,
                                                   embedded_files) &&
      query_event_listener::CacheClassFromFiles(env, activity,
                                                embedded_files) &&
      void_event_listener::CacheClassFromFiles(env, activity, embedded_files) &&
      // Cache method-ids
      cpp_event_listener::CacheMethodIds(env, activity) &&
      document_event_listener::CacheMethodIds(env, activity) &&
      query_event_listener::CacheMethodIds(env, activity) &&
      void_event_listener::CacheMethodIds(env, activity) &&
      // Register natives
      document_event_listener::RegisterNatives(
          env, kDocumentEventListenerNatives,
          FIREBASE_ARRAYSIZE(kDocumentEventListenerNatives)) &&
      query_event_listener::RegisterNatives(
          env, kQueryEventListenerNatives,
          FIREBASE_ARRAYSIZE(kQueryEventListenerNatives)) &&
      void_event_listener::RegisterNatives(
          env, kVoidEventListenerNatives,
          FIREBASE_ARRAYSIZE(kVoidEventListenerNatives));
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void EventListenerInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  // Release embedded classes.
  cpp_event_listener::ReleaseClass(env);
  document_event_listener::ReleaseClass(env);
  query_event_listener::ReleaseClass(env);
  void_event_listener::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
