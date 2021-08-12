/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
