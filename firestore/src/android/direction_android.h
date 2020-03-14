#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DIRECTION_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DIRECTION_ANDROID_H_

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firestore/src/android/query_android.h"

namespace firebase {
namespace firestore {

class DirectionInternal {
 public:
  static jobject ToJavaObject(JNIEnv* env, Query::Direction direction);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);

  static jobject ascending_;
  static jobject descending_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DIRECTION_ANDROID_H_
