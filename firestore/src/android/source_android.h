#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SOURCE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SOURCE_ANDROID_H_

#include <map>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firestore/src/include/firebase/firestore/source.h"

namespace firebase {
namespace firestore {

class SourceInternal {
 public:
  static jobject ToJavaObject(JNIEnv* env, Source source);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);

  static std::map<Source, jobject>* cpp_enum_to_java_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SOURCE_ANDROID_H_
