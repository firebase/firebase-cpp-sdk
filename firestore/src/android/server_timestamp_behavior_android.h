#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SERVER_TIMESTAMP_BEHAVIOR_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SERVER_TIMESTAMP_BEHAVIOR_ANDROID_H_

#include <map>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"

namespace firebase {
namespace firestore {

class ServerTimestampBehaviorInternal {
 public:
  static jobject ToJavaObject(JNIEnv* env,
                              DocumentSnapshot::ServerTimestampBehavior stb);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);

  static std::map<DocumentSnapshot::ServerTimestampBehavior, jobject>*
      cpp_enum_to_java_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SERVER_TIMESTAMP_BEHAVIOR_ANDROID_H_
