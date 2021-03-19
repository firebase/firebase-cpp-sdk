#include "firestore/src/android/timestamp_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "gtest/gtest.h"
#include "firebase/firestore/timestamp.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;

using TimestampTest = FirestoreIntegrationTest;

TEST_F(TimestampTest, Converts) {
  Env env;

  Timestamp timestamp{1234, 5678};
  auto java_timestamp = TimestampInternal::Create(env, timestamp);
  EXPECT_EQ(timestamp, java_timestamp.ToPublic(env));
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
