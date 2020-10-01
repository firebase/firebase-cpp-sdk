#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SERVER_TIMESTAMP_BEHAVIOR_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SERVER_TIMESTAMP_BEHAVIOR_ANDROID_H_

#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class ServerTimestampBehaviorInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static jni::Local<jni::Object> Create(
      jni::Env& env, DocumentSnapshot::ServerTimestampBehavior stb);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SERVER_TIMESTAMP_BEHAVIOR_ANDROID_H_
