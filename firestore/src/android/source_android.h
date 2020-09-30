#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SOURCE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SOURCE_ANDROID_H_

#include "firestore/src/include/firebase/firestore/source.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class SourceInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static jni::Local<jni::Object> Create(jni::Env& env, Source source);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SOURCE_ANDROID_H_
