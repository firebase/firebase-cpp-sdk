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

#include "firebase/firestore.h"
#include "firestore_integration_test.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRTypeTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/TypeTest.java

namespace firebase {
namespace firestore {

class TypeTest : public FirestoreIntegrationTest {
 public:
  // Write the specified data to Firestore as a document and read that document.
  // Check the data read from that document matches with the original data.
  void AssertSuccessfulRoundTrip(MapFieldValue data) {
    DocumentReference reference = Document();
    WriteDocument(reference, data);
    DocumentSnapshot snapshot = ReadDocument(reference);
    EXPECT_TRUE(snapshot.exists());
    EXPECT_EQ(snapshot.GetData(), data);
  }
};

TEST_F(TypeTest, TestCanReadAndWriteNullFields) {
  AssertSuccessfulRoundTrip(
      {{"a", FieldValue::Integer(1)}, {"b", FieldValue::Null()}});
}

TEST_F(TypeTest, TestCanReadAndWriteArrayFields) {
  AssertSuccessfulRoundTrip(
      {{"array", FieldValue::Array(
                     {FieldValue::Integer(1), FieldValue::String("foo"),
                      FieldValue::Map({{"deep", FieldValue::Boolean(true)}}),
                      FieldValue::Null()})}});
}

TEST_F(TypeTest, TestCanReadAndWriteBlobFields) {
  uint8_t blob[3] = {0, 1, 2};
  AssertSuccessfulRoundTrip({{"blob", FieldValue::Blob(blob, 3)}});
}

TEST_F(TypeTest, TestCanReadAndWriteGeoPointFields) {
  AssertSuccessfulRoundTrip({{"geoPoint", FieldValue::GeoPoint({1.23, 4.56})}});
}

TEST_F(TypeTest, TestCanReadAndWriteDateFields) {
  AssertSuccessfulRoundTrip(
      {{"date", FieldValue::Timestamp(Timestamp::FromTimeT(1491847082))}});
}

TEST_F(TypeTest, TestCanReadAndWriteTimestampFields) {
  AssertSuccessfulRoundTrip(
      {{"date", FieldValue::Timestamp({123456, 123456000})}});
}

TEST_F(TypeTest, TestCanReadAndWriteDocumentReferences) {
  AssertSuccessfulRoundTrip({{"a", FieldValue::Integer(42)},
                             {"ref", FieldValue::Reference(Document())}});
}

TEST_F(TypeTest, TestCanReadAndWriteDocumentReferencesInArrays) {
  AssertSuccessfulRoundTrip(
      {{"a", FieldValue::Integer(42)},
       {"refs", FieldValue::Array({FieldValue::Reference(Document())})}});
}

}  // namespace firestore
}  // namespace firebase
