#include "firestore/src/android/timestamp_android.h"

#include <jni.h>

#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"
#include "firebase/firestore/timestamp.h"

namespace firebase {
namespace firestore {

TEST_F(FirestoreIntegrationTest, Converter) {
  JNIEnv* env = app()->GetJNIEnv();

  const Timestamp timestamp{1234, 5678};
  jobject java_timestamp =
      TimestampInternal::TimestampToJavaTimestamp(env, timestamp);
  EXPECT_EQ(timestamp,
            TimestampInternal::JavaTimestampToTimestamp(env, java_timestamp));

  env->DeleteLocalRef(java_timestamp);
}

}  // namespace firestore
}  // namespace firebase
