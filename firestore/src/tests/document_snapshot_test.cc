#include <utility>

#include "firestore/src/include/firebase/firestore.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/common/wrapper_assertions.h"
#include "firestore/src/stub/document_snapshot_stub.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

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
