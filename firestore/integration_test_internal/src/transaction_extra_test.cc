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

#include "app/src/time.h"
#include "firebase/firestore.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#if defined(__ANDROID__)
#include "firestore/src/android/transaction_android.h"
#endif  // defined(__ANDROID__)
#include "firebase_test_framework.h"
// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FSTTransactionTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/TransactionTest.java

namespace firebase {
namespace firestore {

using TransactionExtraTest = FirestoreIntegrationTest;

TEST_F(TransactionExtraTest,
       TestRetriesWhenDocumentThatWasReadWithoutBeingWrittenChanges) {
  DocumentReference doc1 = TestFirestore()->Collection("counter").Document();
  DocumentReference doc2 = TestFirestore()->Collection("counter").Document();
  WriteDocument(doc1, MapFieldValue{{"count", FieldValue::Integer(15)}});
  // Use these two as a portable way to mimic atomic integer.
  Mutex mutex;
  int transaction_runs_count = 0;

  Future<void> future = TestFirestore()->RunTransaction(
      [&doc1, &doc2, &mutex, &transaction_runs_count](
          Transaction& transaction, std::string& error_message) -> Error {
        {
          MutexLock lock(mutex);
          ++transaction_runs_count;
        }
        // Get the first doc.
        Error error = Error::kErrorOk;
        DocumentSnapshot snapshot1 =
            transaction.Get(doc1, &error, &error_message);
        EXPECT_EQ(Error::kErrorOk, error);
        // Do a write outside of the transaction. The first time the
        // transaction is tried, this will bump the version, which
        // will cause the write to doc2 to fail. The second time, it
        // will be a no-op and not bump the version.
        // Now try to update the other doc from within the transaction.
        Await(doc1.Set(MapFieldValue{{"count", FieldValue::Integer(1234)}}));
        // Now try to update the other doc from within the transaction.
        // This should fail once, because we read 15 earlier.
        transaction.Set(doc2,
                        MapFieldValue{{"count", FieldValue::Integer(16)}});
        return Error::kErrorOk;
      });
  Await(future);
  EXPECT_EQ(Error::kErrorOk, future.error());
  EXPECT_EQ(2, transaction_runs_count);
  DocumentSnapshot snapshot = ReadDocument(doc1);
  EXPECT_EQ(1234, snapshot.Get("count").integer_value());
}

TEST_F(TransactionExtraTest, TestReadingADocTwiceWithDifferentVersions) {
  int counter = 0;
  DocumentReference doc = TestFirestore()->Collection("counters").Document();
  WriteDocument(doc, MapFieldValue{{"count", FieldValue::Double(15.0)}});

  // clang-format off
  Future<void> future = TestFirestore()->RunTransaction(
      [&doc, &counter](Transaction& transaction,
                       std::string& error_message) -> Error {
    // clang-format on
    Error error = Error::kErrorOk;
    // Get the doc once.
    DocumentSnapshot snapshot1 = transaction.Get(doc, &error, &error_message);
    EXPECT_EQ(Error::kErrorOk, error);
    // Do a write outside of the transaction. Because the transaction will
    // retry, set the document to a different value each time.
    Await(doc.Set(
        MapFieldValue{{"count", FieldValue::Double(1234.0 + counter)}}));
    ++counter;
    // Get the doc again in the transaction with the new version.
    DocumentSnapshot snapshot2 = transaction.Get(doc, &error, &error_message);
    // We cannot check snapshot2, which is invalid as the second read would
    // have already failed.

    // Now try to update the doc from within the transaction.
    // This should fail, because we read 15 earlier.
    transaction.Set(doc, MapFieldValue{{"count", FieldValue::Double(16.0)}});
    return error;
  });
  Await(future);
  EXPECT_EQ(Error::kErrorAborted, future.error());
  EXPECT_STREQ("Document version changed between two reads.",
               future.error_message());

  DocumentSnapshot snapshot = ReadDocument(doc);
}

}  // namespace firestore
}  // namespace firebase
