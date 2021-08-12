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

#include <utility>

#include "firebase/firestore.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#endif  // defined(__ANDROID__)

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

#if defined(__ANDROID__)

using DocumentSnapshotTest = testing::Test;

TEST_F(DocumentSnapshotTest, Construction) {
  testutil::AssertWrapperConstructionContract<DocumentSnapshot,
                                              DocumentSnapshotInternal>();
}

TEST_F(DocumentSnapshotTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<DocumentSnapshot,
                                            DocumentSnapshotInternal>();
}

#endif

}  // namespace firestore
}  // namespace firebase
