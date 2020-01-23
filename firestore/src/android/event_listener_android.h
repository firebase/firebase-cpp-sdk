#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_

#include <jni.h>

#include "app/src/embedded_file.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/event_listener.h"

namespace firebase {
namespace firestore {

class EventListenerInternal {
 public:
  static void DocumentEventListenerNativeOnEvent(JNIEnv* env, jclass clazz,
                                                 jlong firestore_ptr,
                                                 jlong listener_ptr,
                                                 jobject value, jobject error);
  static void QueryEventListenerNativeOnEvent(JNIEnv* env, jclass clazz,
                                              jlong firestore_ptr,
                                              jlong listener_ptr, jobject value,
                                              jobject error);

  static jobject EventListenerToJavaEventListener(
      JNIEnv* env, FirestoreInternal* firestore,
      EventListener<DocumentSnapshot>* listener);

  static jobject EventListenerToJavaEventListener(
      JNIEnv* env, FirestoreInternal* firestore,
      EventListener<QuerySnapshot>* listener);

 private:
  friend class FirestoreInternal;

  static bool InitializeEmbeddedClasses(
      App* app, const std::vector<internal::EmbeddedFile>* embedded_files);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_EVENT_LISTENER_ANDROID_H_
