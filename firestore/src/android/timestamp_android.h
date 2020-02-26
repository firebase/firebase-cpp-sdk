#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TIMESTAMP_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TIMESTAMP_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "firebase/firestore/timestamp.h"

namespace firebase {
namespace firestore {

// This is the non-wrapper Android implementation of Timestamp. Since Timestamp
// has most of their method inlined, we use it directly instead of wrapping
// around a Java Timestamp object. We still need the helper functions to convert
// between the two types. In addition, we also need proper initializer and
// terminator for the Java class cache/uncache.
class TimestampInternal {
 public:
  using ApiType = Timestamp;

  // Convert a C++ Timestamp into a Java Timestamp.
  static jobject TimestampToJavaTimestamp(JNIEnv* env,
                                          const Timestamp& timestamp);

  // Convert a Java Timestamp into a C++ Timestamp.
  static Timestamp JavaTimestampToTimestamp(JNIEnv* env, jobject obj);

  // Gets the class object of Java Timestamp class.
  static jclass GetClass();

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TIMESTAMP_ANDROID_H_
