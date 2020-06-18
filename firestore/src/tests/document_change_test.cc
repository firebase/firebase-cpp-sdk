#include "firestore/src/common/wrapper_assertions.h"
#include "firestore/src/include/firebase/firestore.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_change_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/document_change_stub.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

using DocumentChangeTest = testing::Test;

TEST_F(DocumentChangeTest, Construction) {
  testutil::AssertWrapperConstructionContract<DocumentChange,
                                              DocumentChangeInternal>();
}

TEST_F(DocumentChangeTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<DocumentChange,
                                            DocumentChangeInternal>();
}

#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

}  // namespace firestore
}  // namespace firebase
