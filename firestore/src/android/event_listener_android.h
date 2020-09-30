#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/event_listener.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class EventListenerInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static void DocumentEventListenerNativeOnEvent(JNIEnv* env, jclass clazz,
                                                 jlong firestore_ptr,
                                                 jlong listener_ptr,
                                                 jobject value, jobject error);
  static void QueryEventListenerNativeOnEvent(JNIEnv* env, jclass clazz,
                                              jlong firestore_ptr,
                                              jlong listener_ptr, jobject value,
                                              jobject error);
  static void VoidEventListenerNativeOnEvent(JNIEnv* env, jclass clazz,
                                             jlong listener_ptr);

  static jni::Local<jni::Object> Create(
      jni::Env& env, FirestoreInternal* firestore,
      EventListener<DocumentSnapshot>* listener);

  static jobject EventListenerToJavaEventListener(
      JNIEnv* env, FirestoreInternal* firestore,
      EventListener<DocumentSnapshot>* listener);

  static jni::Local<jni::Object> Create(jni::Env& env,
                                        FirestoreInternal* firestore,
                                        EventListener<QuerySnapshot>* listener);

  static jobject EventListenerToJavaEventListener(
      JNIEnv* env, FirestoreInternal* firestore,
      EventListener<QuerySnapshot>* listener);

  static jni::Local<jni::Object> Create(jni::Env& env,
                                        EventListener<void>* listener);

  static jobject EventListenerToJavaRunnable(JNIEnv* env,
                                             EventListener<void>* listener);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_
