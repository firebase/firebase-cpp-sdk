// Copyright 2021 Google LLC

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
