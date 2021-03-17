#include <utility>
#include <vector>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRArrayTransformTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/ArrayTransformsTest.java
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/ArrayTransformServerApplicationTest.java

namespace firebase {
namespace firestore {

class ArrayTransformTest : public FirestoreIntegrationTest {
 protected:
  void SetUp() override {
    document_ = Document();
    registration_ = accumulator_.listener()->AttachTo(
        &document_, MetadataChanges::kInclude);

    // Wait for initial null snapshot to avoid potential races.
    DocumentSnapshot snapshot = accumulator_.AwaitServerEvent();
    EXPECT_FALSE(snapshot.exists());
  }

  void TearDown() override { registration_.Remove(); }

  void WriteInitialData(const MapFieldValue& data) {
    Await(document_.Set(data));
    ExpectLocalAndRemoteEvent(data);
  }

  void ExpectLocalAndRemoteEvent(const MapFieldValue& data) {
    EXPECT_THAT(accumulator_.AwaitLocalEvent().GetData(),
                testing::ContainerEq(data));
    EXPECT_THAT(accumulator_.AwaitRemoteEvent().GetData(),
                testing::ContainerEq(data));
  }

  DocumentReference document_;
  EventAccumulator<DocumentSnapshot> accumulator_;
  ListenerRegistration registration_;
};

class ArrayTransformServerApplicationTest : public FirestoreIntegrationTest {
 protected:
  void SetUp() override { document_ = Document(); }

  DocumentReference document_;
};

TEST_F(ArrayTransformTest, CreateDocumentWithArrayUnion) {
  Await(document_.Set(MapFieldValue{
      {"array", FieldValue::ArrayUnion(
                    {FieldValue::Integer(1), FieldValue::Integer(2)})}}));
  ExpectLocalAndRemoteEvent(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(1), FieldValue::Integer(2)})}});
}

TEST_F(ArrayTransformTest, AppendToArrayViaUpdate) {
  WriteInitialData(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(1), FieldValue::Integer(3)})}});
  Await(document_.Update(MapFieldValue{
      {"array",
       FieldValue::ArrayUnion({FieldValue::Integer(2), FieldValue::Integer(1),
                               FieldValue::Integer(4)})}}));
  ExpectLocalAndRemoteEvent(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(1), FieldValue::Integer(3),
                          FieldValue::Integer(2), FieldValue::Integer(4)})}});
}

TEST_F(ArrayTransformTest, AppendToArrayViaMergeSet) {
  WriteInitialData(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(1), FieldValue::Integer(3)})}});
  Await(document_.Set(MapFieldValue{{"array", FieldValue::ArrayUnion(
                                                  {FieldValue::Integer(2),
                                                   FieldValue::Integer(1),
                                                   FieldValue::Integer(4)})}},
                      SetOptions::Merge()));
  ExpectLocalAndRemoteEvent(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(1), FieldValue::Integer(3),
                          FieldValue::Integer(2), FieldValue::Integer(4)})}});
}

TEST_F(ArrayTransformTest, AppendObjectToArrayViaUpdate) {
  WriteInitialData(MapFieldValue{
      {"array", FieldValue::Array(
                    {FieldValue::Map({{"a", FieldValue::String("hi")}})})}});
  Await(document_.Update(MapFieldValue{
      {"array",
       FieldValue::ArrayUnion(
           {{FieldValue::Map({{"a", FieldValue::String("hi")}})},
            {FieldValue::Map({{"a", FieldValue::String("bye")}})}})}}));
  ExpectLocalAndRemoteEvent(MapFieldValue{
      {"array", FieldValue::Array(
                    {{FieldValue::Map({{"a", FieldValue::String("hi")}})},
                     {FieldValue::Map({{"a", FieldValue::String("bye")}})}})}});
}

TEST_F(ArrayTransformTest, RemoveFromArrayViaUpdate) {
  WriteInitialData(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(1), FieldValue::Integer(3),
                          FieldValue::Integer(1), FieldValue::Integer(3)})}});
  Await(document_.Update(MapFieldValue{
      {"array", FieldValue::ArrayRemove(
                    {FieldValue::Integer(1), FieldValue::Integer(4)})}}));
  ExpectLocalAndRemoteEvent(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(3), FieldValue::Integer(3)})}});
}

TEST_F(ArrayTransformTest, RemoveFromArrayViaMergeSet) {
  WriteInitialData(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(1), FieldValue::Integer(3),
                          FieldValue::Integer(1), FieldValue::Integer(3)})}});
  Await(document_.Set(MapFieldValue{{"array", FieldValue::ArrayRemove(
                                                  {FieldValue::Integer(1),
                                                   FieldValue::Integer(4)})}},
                      SetOptions::Merge()));
  ExpectLocalAndRemoteEvent(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(3), FieldValue::Integer(3)})}});
}

TEST_F(ArrayTransformTest, RemoveObjectFromArrayViaUpdate) {
  WriteInitialData(MapFieldValue{
      {"array", FieldValue::Array(
                    {FieldValue::Map({{"a", FieldValue::String("hi")}}),
                     FieldValue::Map({{"a", FieldValue::String("bye")}})})}});
  Await(document_.Update(MapFieldValue{
      {"array", FieldValue::ArrayRemove(
                    {{FieldValue::Map({{"a", FieldValue::String("hi")}})}})}}));
  ExpectLocalAndRemoteEvent(MapFieldValue{
      {"array", FieldValue::Array(
                    {{FieldValue::Map({{"a", FieldValue::String("bye")}})}})}});
}

TEST_F(ArrayTransformServerApplicationTest, SetWithNoCachedBaseDoc) {
  Await(document_.Set(MapFieldValue{
      {"array", FieldValue::ArrayUnion(
                    {FieldValue::Integer(1), FieldValue::Integer(2)})}}));
  DocumentSnapshot snapshot = *Await(document_.Get(Source::kCache));
  EXPECT_THAT(snapshot.GetData(),
              testing::ContainerEq(MapFieldValue{
                  {"array", FieldValue::Array({FieldValue::Integer(1),
                                               FieldValue::Integer(2)})}}));
}

TEST_F(ArrayTransformServerApplicationTest, UpdateWithNoCachedBaseDoc) {
  // Write an initial document in an isolated Firestore instance so it's not
  // stored in our cache.
  Await(TestFirestore("isolated")
            ->Document(document_.path())
            .Set(MapFieldValue{
                {"array", FieldValue::Array({FieldValue::Integer(42)})}}));

  Await(document_.Update(MapFieldValue{
      {"array", FieldValue::ArrayUnion(
                    {FieldValue::Integer(1), FieldValue::Integer(2)})}}));

  // Nothing should be cached since it was an update and we had no base doc.
  Future<DocumentSnapshot> future = document_.Get(Source::kCache);
  Await(future);
  EXPECT_EQ(Error::kErrorUnavailable, future.error());
}

TEST_F(ArrayTransformServerApplicationTest, MergeSetWithNoCachedBaseDoc) {
  // Write an initial document in an isolated Firestore instance so it's not
  // stored in our cache.
  Await(TestFirestore("isolated")
            ->Document(document_.path())
            .Set(MapFieldValue{
                {"array", FieldValue::Array({FieldValue::Integer(42)})}}));

  Await(document_.Set(MapFieldValue{{"array", FieldValue::ArrayUnion(
                                                  {FieldValue::Integer(1),
                                                   FieldValue::Integer(2)})}},
                      SetOptions::Merge()));
  // Document will be cached but we'll be missing 42.
  DocumentSnapshot snapshot = *Await(document_.Get(Source::kCache));
  EXPECT_THAT(snapshot.GetData(),
              testing::ContainerEq(MapFieldValue{
                  {"array", FieldValue::Array({FieldValue::Integer(1),
                                               FieldValue::Integer(2)})}}));
}

TEST_F(ArrayTransformServerApplicationTest,
       UpdateWithCachedBaseDocUsingArrayUnion) {
  Await(document_.Set(
      MapFieldValue{{"array", FieldValue::Array({FieldValue::Integer(42)})}}));
  Await(document_.Update(MapFieldValue{
      {"array", FieldValue::ArrayUnion(
                    {FieldValue::Integer(1), FieldValue::Integer(2)})}}));
  DocumentSnapshot snapshot = *Await(document_.Get(Source::kCache));
  EXPECT_THAT(snapshot.GetData(),
              testing::ContainerEq(MapFieldValue{
                  {"array", FieldValue::Array({FieldValue::Integer(42),
                                               FieldValue::Integer(1),
                                               FieldValue::Integer(2)})}}));
}

TEST_F(ArrayTransformServerApplicationTest,
       UpdateWithCachedBaseDocUsingArrayRemove) {
  Await(document_.Set(MapFieldValue{
      {"array",
       FieldValue::Array({FieldValue::Integer(42), FieldValue::Integer(1L),
                          FieldValue::Integer(2L)})}}));
  Await(document_.Update(MapFieldValue{
      {"array", FieldValue::ArrayRemove(
                    {FieldValue::Integer(1), FieldValue::Integer(2)})}}));
  DocumentSnapshot snapshot = *Await(document_.Get(Source::kCache));
  EXPECT_THAT(snapshot.GetData(),
              testing::ContainerEq(MapFieldValue{
                  {"array", FieldValue::Array({FieldValue::Integer(42)})}}));
}

}  // namespace firestore
}  // namespace firebase
