// Copyright 2021 Google LLC

#include <utility>

#include "firebase/firestore.h"
#include "firestore/src/common/wrapper_assertions.h"
#if defined(__ANDROID__)
#include "firestore/src/android/query_snapshot_android.h"
#endif  // defined(__ANDROID__)

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

#if defined(__ANDROID__)

using QuerySnapshotTest = testing::Test;

TEST_F(QuerySnapshotTest, Construction) {
  testutil::AssertWrapperConstructionContract<QuerySnapshot,
                                              QuerySnapshotInternal>();
}

TEST_F(QuerySnapshotTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<QuerySnapshot,
                                            QuerySnapshotInternal>();
}

#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
