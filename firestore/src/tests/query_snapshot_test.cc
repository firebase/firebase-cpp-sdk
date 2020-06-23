#include <utility>

#include "firestore/src/common/wrapper_assertions.h"
#include "firestore/src/include/firebase/firestore.h"
#if defined(__ANDROID__)
#include "firestore/src/android/query_snapshot_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/query_snapshot_stub.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

using QuerySnapshotTest = testing::Test;

TEST_F(QuerySnapshotTest, Construction) {
  testutil::AssertWrapperConstructionContract<QuerySnapshot,
                                              QuerySnapshotInternal>();
}

TEST_F(QuerySnapshotTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<QuerySnapshot,
                                            QuerySnapshotInternal>();
}

#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

}  // namespace firestore
}  // namespace firebase
