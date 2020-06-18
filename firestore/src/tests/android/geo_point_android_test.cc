#include "firestore/src/android/geo_point_android.h"

#include <jni.h>

#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"
#include "firebase/firestore/geo_point.h"

namespace firebase {
namespace firestore {

TEST_F(FirestoreIntegrationTest, Converter) {
  JNIEnv* env = app()->GetJNIEnv();

  const GeoPoint point{12.0, 34.0};
  jobject java_point = GeoPointInternal::GeoPointToJavaGeoPoint(env, point);
  EXPECT_EQ(point, GeoPointInternal::JavaGeoPointToGeoPoint(env, java_point));

  env->DeleteLocalRef(java_point);
}

}  // namespace firestore
}  // namespace firebase
