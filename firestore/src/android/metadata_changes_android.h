#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_METADATA_CHANGES_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_METADATA_CHANGES_ANDROID_H_

#include "firestore/src/include/firebase/firestore/metadata_changes.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class MetadataChangesInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static jni::Local<jni::Object> Create(jni::Env& env,
                                        MetadataChanges metadata_changes);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_METADATA_CHANGES_ANDROID_H_
