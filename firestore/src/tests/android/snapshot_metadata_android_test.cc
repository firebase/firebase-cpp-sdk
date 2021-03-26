#include "firestore/src/android/snapshot_metadata_android.h"

#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using jni::Class;
using jni::Env;
using jni::Local;

TEST_F(FirestoreIntegrationTest, Converts) {
  Env env;

  Local<Class> clazz =
      env.FindClass("com/google/firebase/firestore/SnapshotMetadata");
  jmethodID ctor = env.GetMethodId(clazz, "<init>", "(ZZ)V");

  auto java_metadata = env.New<SnapshotMetadataInternal>(
      clazz, ctor, /*has_pending_writes=*/true, /*is_from_cache=*/false);
  SnapshotMetadata result = java_metadata.ToPublic(env);
  EXPECT_TRUE(result.has_pending_writes());
  EXPECT_FALSE(result.is_from_cache());

  java_metadata = env.New<SnapshotMetadataInternal>(
      clazz, ctor, /*has_pending_writes=*/false,
      /*is_from_cache=*/true);
  result = java_metadata.ToPublic(env);
  EXPECT_FALSE(result.has_pending_writes());
  EXPECT_TRUE(result.is_from_cache());
}

}  // namespace firestore
}  // namespace firebase
