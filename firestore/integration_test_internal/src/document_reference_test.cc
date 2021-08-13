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
#include "firestore/src/common/wrapper_assertions.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#if defined(__ANDROID__)
#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/document_reference_android.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

#if defined(__ANDROID__)

using DocumentReferenceTest = FirestoreIntegrationTest;

TEST_F(DocumentReferenceTest, Construction) {
  testutil::AssertWrapperConstructionContract<DocumentReference,
                                              DocumentReferenceInternal>();
}

TEST_F(DocumentReferenceTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<DocumentReference,
                                            DocumentReferenceInternal>();
}

#endif  // defined(__ANDROID__)

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
