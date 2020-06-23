#include <utility>

#include "firestore/src/common/wrapper_assertions.h"
#include "firestore/src/include/firebase/firestore.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_reference_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/document_reference_stub.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

using DocumentReferenceTest = testing::Test;

TEST_F(DocumentReferenceTest, Construction) {
  testutil::AssertWrapperConstructionContract<DocumentReference,
                                              DocumentReferenceInternal>();
}

TEST_F(DocumentReferenceTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<DocumentReference,
                                            DocumentReferenceInternal>();
}

#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

}  // namespace firestore
}  // namespace firebase
