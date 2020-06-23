#include <utility>

#include "firestore/src/common/wrapper_assertions.h"
#include "firestore/src/include/firebase/firestore.h"
#if defined(__ANDROID__)
#include "firestore/src/android/collection_reference_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/collection_reference_stub.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

using CollectionReferenceTest = testing::Test;

TEST_F(CollectionReferenceTest, Construction) {
  testutil::AssertWrapperConstructionContract<CollectionReference,
                                              CollectionReferenceInternal>();
}

TEST_F(CollectionReferenceTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<CollectionReference,
                                            CollectionReferenceInternal>();
}

#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

}  // namespace firestore
}  // namespace firebase
