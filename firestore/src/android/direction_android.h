#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DIRECTION_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DIRECTION_ANDROID_H_

#include "firestore/src/android/query_android.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class DirectionInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static jni::Local<jni::Object> Create(jni::Env& env,
                                        Query::Direction direction);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DIRECTION_ANDROID_H_
