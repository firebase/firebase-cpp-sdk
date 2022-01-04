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

#include "firestore/src/android/snapshot_metadata_android.h"

#include "android/firestore_integration_test_android.h"
#include "firebase_test_framework.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/jni/declaration.h"
#include "firestore/src/jni/env.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using SnapshotMetadataAndroidTest = FirestoreAndroidIntegrationTest;

using jni::Class;
using jni::Constructor;
using jni::Env;
using jni::Local;

TEST_F(SnapshotMetadataAndroidTest, Converts) {
  Env env;
  Constructor<SnapshotMetadataInternal> ctor("(ZZ)V");
  loader().LoadClass("com/google/firebase/firestore/SnapshotMetadata", ctor);
  ASSERT_TRUE(loader().ok());

  {
    auto java_metadata =
        env.New(ctor, /*has_pending_writes=*/true, /*is_from_cache=*/false);
    SnapshotMetadata result = java_metadata.ToPublic(env);
    EXPECT_TRUE(env.ok());
    EXPECT_TRUE(result.has_pending_writes());
    EXPECT_FALSE(result.is_from_cache());
  }

  {
    auto java_metadata =
        env.New(ctor, /*has_pending_writes=*/false, /*is_from_cache=*/true);
    SnapshotMetadata result = java_metadata.ToPublic(env);
    EXPECT_TRUE(env.ok());
    EXPECT_FALSE(result.has_pending_writes());
    EXPECT_TRUE(result.is_from_cache());
  }
}

}  // namespace firestore
}  // namespace firebase
