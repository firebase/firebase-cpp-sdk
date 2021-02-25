#include <utility>

#include "firestore/src/common/wrapper_assertions.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

#if defined(__ANDROID__)
#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/document_reference_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/document_reference_stub.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

using DocumentReferenceTest = FirestoreIntegrationTest;

TEST_F(DocumentReferenceTest, Construction) {
  testutil::AssertWrapperConstructionContract<DocumentReference,
                                              DocumentReferenceInternal>();
}

TEST_F(DocumentReferenceTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<DocumentReference,
                                            DocumentReferenceInternal>();
}

#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

#if defined(__ANDROID__)

TEST_F(DocumentReferenceTest, RecoverFirestore) {
  jni::Env env = FirestoreInternal::GetEnv();

  DocumentReference result =
      DocumentReferenceInternal::Create(env, jni::Object());
  EXPECT_FALSE(result.is_valid());

  Firestore* db = TestFirestore();
  DocumentReference doc = Document();
  ASSERT_EQ(db, doc.firestore());  // Sanity check

  jni::Object doc_java = GetInternal(doc)->ToJava();
  result = DocumentReferenceInternal::Create(env, doc_java);
  ASSERT_EQ(db, result.firestore());
}

#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
