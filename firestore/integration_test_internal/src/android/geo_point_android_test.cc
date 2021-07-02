// Copyright 2021 Google LLC

#include "firestore/src/android/geo_point_android.h"

#include "firebase/firestore/geo_point.h"
#include "firestore/src/jni/env.h"
#include "firestore_integration_test.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;

using GeoPointTest = FirestoreIntegrationTest;

TEST_F(GeoPointTest, Converts) {
  Env env;

  GeoPoint point{12.0, 34.0};
  auto java_point = GeoPointInternal::Create(env, point);
  EXPECT_EQ(point, java_point.ToPublic(env));
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
