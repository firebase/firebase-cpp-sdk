#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SNAPSHOT_METADATA_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SNAPSHOT_METADATA_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"

namespace firebase {
namespace firestore {

// This is the non-wrapper Android implementation of SnapshotMetadata. Since
// SnapshotMetadata has all methods inlined, we use it directly instead of
// wrapping around a Java SnapshotMetadata object. We still need the helper
// functions to convert between the two types. In addition, we also need proper
// initializer and terminator for the Java class cache/uncache.
class SnapshotMetadataInternal {
 public:
  using ApiType = SnapshotMetadata;

  // Convert a C++ SnapshotMetadata into a Java SnapshotMetadata.
  static jobject SnapshotMetadataToJavaSnapshotMetadata(
      JNIEnv* env, const SnapshotMetadata& metadata);

  // Convert a Java SnapshotMetadata into a C++ SnapshotMetadata and release the
  // reference to the jobject.
  static SnapshotMetadata JavaSnapshotMetadataToSnapshotMetadata(JNIEnv* env,
                                                                 jobject obj);

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_SNAPSHOT_METADATA_ANDROID_H_
