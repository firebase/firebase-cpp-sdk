#include "firestore/src/android/snapshot_metadata_android.h"

#include <jni.h>

#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

TEST_F(FirestoreIntegrationTest, ConvertHasPendingWrites) {
  JNIEnv* env = app()->GetJNIEnv();

  const SnapshotMetadata has_pending_writes{/*has_pending_writes=*/true,
                                            /*is_from_cache=*/false};
  // The converter will delete local reference for us.
  const SnapshotMetadata result =
      SnapshotMetadataInternal::JavaSnapshotMetadataToSnapshotMetadata(
          env, SnapshotMetadataInternal::SnapshotMetadataToJavaSnapshotMetadata(
                   env, has_pending_writes));
  EXPECT_TRUE(result.has_pending_writes());
  EXPECT_FALSE(result.is_from_cache());
}

TEST_F(FirestoreIntegrationTest, ConvertIsFromCache) {
  JNIEnv* env = app()->GetJNIEnv();

  const SnapshotMetadata is_from_cache{/*has_pending_writes=*/false,
                                       /*is_from_cache=*/true};
  // The converter will delete local reference for us.
  const SnapshotMetadata result =
      SnapshotMetadataInternal::JavaSnapshotMetadataToSnapshotMetadata(
          env, SnapshotMetadataInternal::SnapshotMetadataToJavaSnapshotMetadata(
                   env, is_from_cache));
  EXPECT_FALSE(result.has_pending_writes());
  EXPECT_TRUE(result.is_from_cache());
}

}  // namespace firestore
}  // namespace firebase
