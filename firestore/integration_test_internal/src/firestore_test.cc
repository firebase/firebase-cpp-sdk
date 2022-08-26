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

#include <algorithm>
#include <future>
#include <memory>
#include <stdexcept>

#if defined(__ANDROID__)
#include "android/firestore_integration_test_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/jni_runnable_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"
#endif  // defined(__ANDROID__)

#include "app/src/include/firebase/internal/mutex.h"
#include "auth/src/include/firebase/auth.h"
#include "firestore/src/common/macros.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/event_accumulator.h"
#include "util/future_test_util.h"
#if !defined(__ANDROID__)
#include "Firestore/core/src/util/autoid.h"
#else
#include "android/util_autoid.h"
#endif  // !defined(__ANDROID__)
#include "Firestore/core/src/util/firestore_exceptions.h"
#include "firebase_test_framework.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRDatabaseTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/FirestoreTest.java
// Some test cases are named differently between iOS and Android. Here we choose
// the most descriptive names.

namespace firebase {
namespace firestore {

using ::firebase::auth::Auth;
using ::testing::ContainerEq;
using ::testing::HasSubstr;

TEST_F(FirestoreIntegrationTest, GetInstance) {
  // Create App.
  App* app = this->app();
  EXPECT_NE(nullptr, app);

  // Get an instance.
  InitResult result;
  Firestore* instance = Firestore::GetInstance(app, &result);
  EXPECT_EQ(kInitResultSuccess, result);
  EXPECT_NE(nullptr, instance);
  EXPECT_EQ(app, instance->app());

  Auth* auth = Auth::GetAuth(app);

  // Tests normally create instances outside of those managed by
  // Firestore::GetInstance. This means that in this case `instance` is a new
  // one unmanaged by the test framework. If both the implicit instance and this
  // instance were started they would try to use the same underlying database
  // and would fail.
  delete instance;

  // Firestore calls Auth::GetAuth, which implicitly creates an auth instance.
  // Even though app is cleaned up automatically, the Auth instance is not.
  // TODO(mcg): Figure out why App's CleanupNotifier doesn't handle Auth.
  delete auth;
}

// Sanity test for stubs.
TEST_F(FirestoreIntegrationTest, TestCanCreateCollectionAndDocumentReferences) {
  Firestore* db = TestFirestore();
  CollectionReference c = db->Collection("a/b/c").Document("d").Parent();
  DocumentReference d = db->Document("a/b").Collection("c/d/e").Parent();

  CollectionReference(c).Document();
  DocumentReference(d).Parent();

  CollectionReference(std::move(c)).Document();
  DocumentReference(std::move(d)).Parent();

  // If any of these assert, the test will fail.
}

TEST_F(FirestoreIntegrationTest, TestCanReadNonExistentDocuments) {
  DocumentReference doc = Collection("rooms").Document();

  DocumentSnapshot snap = ReadDocument(doc);
  ASSERT_FALSE(snap.exists());
  EXPECT_THAT(snap.GetData(), ContainerEq(MapFieldValue()));
}

TEST_F(FirestoreIntegrationTest, TestCanUpdateAnExistingDocument) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(document.Update(
      MapFieldValue{{"desc", FieldValue::String("NewDescription")},
                    {"owner.email", FieldValue::String("new@xyz.com")}}));
  DocumentSnapshot doc = ReadDocument(document);
  EXPECT_TRUE(doc.exists());
  EXPECT_THAT(
      doc.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("NewDescription")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Jonny")},
                            {"email", FieldValue::String("new@xyz.com")}})}}));
}

TEST_F(FirestoreIntegrationTest, TestCanUpdateAnUnknownDocument) {
  DocumentReference writer_reference =
      TestFirestore("writer")->Collection("collection").Document();
  DocumentReference reader_reference = TestFirestore("reader")
                                           ->Collection("collection")
                                           .Document(writer_reference.id());
  Await(writer_reference.Set(MapFieldValue{{"a", FieldValue::String("a")}}));
  Await(reader_reference.Update(MapFieldValue{{"b", FieldValue::String("b")}}));

  DocumentSnapshot writer_snapshot =
      *Await(writer_reference.Get(Source::kCache));
  EXPECT_TRUE(writer_snapshot.exists());
  EXPECT_THAT(writer_snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::String("a")}}));
  EXPECT_TRUE(writer_snapshot.metadata().is_from_cache());

  Future<DocumentSnapshot> future = reader_reference.Get(Source::kCache);
  Await(future);
  EXPECT_EQ(Error::kErrorUnavailable, future.error());

  writer_snapshot = ReadDocument(writer_reference);
  EXPECT_TRUE(writer_snapshot.exists());
  EXPECT_THAT(writer_snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::String("a")},
                                        {"b", FieldValue::String("b")}}));
  EXPECT_FALSE(writer_snapshot.metadata().is_from_cache());
  DocumentSnapshot reader_snapshot = ReadDocument(reader_reference);
  EXPECT_TRUE(reader_snapshot.exists());
  EXPECT_THAT(reader_snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::String("a")},
                                        {"b", FieldValue::String("b")}}));
  EXPECT_FALSE(reader_snapshot.metadata().is_from_cache());
}

TEST_F(FirestoreIntegrationTest, TestCanOverwriteAnExistingDocumentUsingSet) {
  DocumentReference document = Collection("rooms").Document();
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner.data",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(document.Set(MapFieldValue{
      {"updated", FieldValue::Boolean(true)},
      {"owner.data",
       FieldValue::Map({{"name", FieldValue::String("Sebastian")}})}}));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"updated", FieldValue::Boolean(true)},
          {"owner.data",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")}})}}));
}

TEST_F(FirestoreIntegrationTest,
       TestCanMergeDataWithAnExistingDocumentUsingSet) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner.data",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(document.Set(
      MapFieldValue{
          {"updated", FieldValue::Boolean(true)},
          {"owner.data",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")}})}},
      SetOptions::Merge()));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("Description")},
          {"updated", FieldValue::Boolean(true)},
          {"owner.data",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                            {"email", FieldValue::String("abc@xyz.com")}})}}));
}

TEST_F(FirestoreIntegrationTest, TestCanMergeServerTimestamps) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{{"untouched", FieldValue::Boolean(true)}}));
  Await(document.Set(
      MapFieldValue{{"time", FieldValue::ServerTimestamp()},
                    {"nested", FieldValue::Map(
                                   {{"time", FieldValue::ServerTimestamp()}})}},
      SetOptions::Merge()));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.Get("untouched").boolean_value());
  EXPECT_TRUE(snapshot.Get("time").is_timestamp());
  EXPECT_TRUE(snapshot.Get("nested.time").is_timestamp());
}

TEST_F(FirestoreIntegrationTest, TestCanMergeEmptyObject) {
  DocumentReference document = Document();
  EventAccumulator<DocumentSnapshot> accumulator;
  ListenerRegistration registration =
      accumulator.listener()->AttachTo(&document);
  accumulator.Await();

  document.Set(MapFieldValue{});
  DocumentSnapshot snapshot = accumulator.Await();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(), ContainerEq(MapFieldValue{}));

  Await(document.Set(MapFieldValue{{"a", FieldValue::Map({})}},
                     SetOptions::MergeFields({"a"})));
  snapshot = accumulator.Await();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Map({})}}));

  Await(document.Set(MapFieldValue{{"b", FieldValue::Map({})}},
                     SetOptions::Merge()));
  snapshot = accumulator.Await();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Map({})},
                                        {"b", FieldValue::Map({})}}));

  auto* snapshot_ptr = Await(document.Get(Source::kServer));
  ASSERT_TRUE(snapshot_ptr);
  snapshot = *snapshot_ptr;
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Map({})},
                                        {"b", FieldValue::Map({})}}));
  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestCanDeleteFieldUsingMerge) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"untouched", FieldValue::Boolean(true)},
      {"foo", FieldValue::String("bar")},
      {"nested", FieldValue::Map({{"untouched", FieldValue::Boolean(true)},
                                  {"foo", FieldValue::String("bar")}})}}));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.Get("untouched").boolean_value());
  EXPECT_TRUE(snapshot.Get("nested.untouched").boolean_value());
  EXPECT_TRUE(snapshot.Get("foo").is_valid());
  EXPECT_TRUE(snapshot.Get("nested.foo").is_valid());

  Await(document.Set(
      MapFieldValue{{"foo", FieldValue::Delete()},
                    {"nested", FieldValue::Map(MapFieldValue{
                                   {"foo", FieldValue::Delete()}})}},
      SetOptions::Merge()));
  snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.Get("untouched").boolean_value());
  EXPECT_TRUE(snapshot.Get("nested.untouched").boolean_value());
  EXPECT_FALSE(snapshot.Get("foo").is_valid());
  EXPECT_FALSE(snapshot.Get("nested.foo").is_valid());
}

TEST_F(FirestoreIntegrationTest, TestCanDeleteFieldUsingMergeFields) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"untouched", FieldValue::Boolean(true)},
      {"foo", FieldValue::String("bar")},
      {"inner", FieldValue::Map({{"removed", FieldValue::Boolean(true)},
                                 {"foo", FieldValue::String("bar")}})},
      {"nested", FieldValue::Map({{"untouched", FieldValue::Boolean(true)},
                                  {"foo", FieldValue::String("bar")}})}}));
  Await(document.Set(
      MapFieldValue{
          {"foo", FieldValue::Delete()},
          {"inner", FieldValue::Map({{"foo", FieldValue::Delete()}})},
          {"nested", FieldValue::Map({{"untouched", FieldValue::Delete()},
                                      {"foo", FieldValue::Delete()}})}},
      SetOptions::MergeFields({"foo", "inner", "nested.foo"})));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"untouched", FieldValue::Boolean(true)},
          {"inner", FieldValue::Map({})},
          {"nested",
           FieldValue::Map({{"untouched", FieldValue::Boolean(true)}})}}));
}

TEST_F(FirestoreIntegrationTest, TestCanSetServerTimestampsUsingMergeFields) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"untouched", FieldValue::Boolean(true)},
      {"foo", FieldValue::String("bar")},
      {"nested", FieldValue::Map({{"untouched", FieldValue::Boolean(true)},
                                  {"foo", FieldValue::String("bar")}})}}));
  Await(document.Set(
      MapFieldValue{
          {"foo", FieldValue::ServerTimestamp()},
          {"inner", FieldValue::Map({{"foo", FieldValue::ServerTimestamp()}})},
          {"nested",
           FieldValue::Map({{"foo", FieldValue::ServerTimestamp()}})}},
      SetOptions::MergeFields({"foo", "inner", "nested.foo"})));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_TRUE(snapshot.Get("foo").is_timestamp());
  EXPECT_TRUE(snapshot.Get("inner.foo").is_timestamp());
  EXPECT_TRUE(snapshot.Get("nested.foo").is_timestamp());
}

TEST_F(FirestoreIntegrationTest, TestMergeReplacesArrays) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"untouched", FieldValue::Boolean(true)},
      {"data", FieldValue::String("old")},
      {"topLevel", FieldValue::Array(
                       {FieldValue::String("old"), FieldValue::String("old")})},
      {"mapInArray", FieldValue::Array({FieldValue::Map(
                         {{"data", FieldValue::String("old")}})})}}));
  Await(document.Set(
      MapFieldValue{
          {"data", FieldValue::String("new")},
          {"topLevel", FieldValue::Array({FieldValue::String("new")})},
          {"mapInArray", FieldValue::Array({FieldValue::Map(
                             {{"data", FieldValue::String("new")}})})}},
      SetOptions::Merge()));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"untouched", FieldValue::Boolean(true)},
          {"data", FieldValue::String("new")},
          {"topLevel", FieldValue::Array({FieldValue::String("new")})},
          {"mapInArray", FieldValue::Array({FieldValue::Map(
                             {{"data", FieldValue::String("new")}})})}}));
}

TEST_F(FirestoreIntegrationTest,
       TestCanDeepMergeDataWithAnExistingDocumentUsingSet) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"owner.data",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("old@xyz.com")}})}}));
  Await(document.Set(
      MapFieldValue{
          {"desc", FieldValue::String("NewDescription")},
          {"owner.data",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                            {"email", FieldValue::String("new@xyz.com")}})}},
      SetOptions::MergeFieldPaths({{"desc"}, {"owner.data", "name"}})));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("NewDescription")},
          {"owner.data",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                            {"email", FieldValue::String("old@xyz.com")}})}}));
}

#if defined(__ANDROID__) && FIRESTORE_HAVE_EXCEPTIONS
// TODO(b/136012313): iOS currently doesn't rethrow native exceptions as C++
// exceptions.
TEST_F(FirestoreIntegrationTest, TestFieldMaskCannotContainMissingFields) {
  DocumentReference document = Collection("rooms").Document();
  try {
    document.Set(MapFieldValue{{"desc", FieldValue::String("NewDescription")}},
                 SetOptions::MergeFields({"desc", "owner"}));
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_STREQ(
        "Field 'owner' is specified in your field mask but not in your input "
        "data.",
        exception.what());
  }
}
#endif  // defined(__ANDROID__) && FIRESTORE_HAVE_EXCEPTIONS

TEST_F(FirestoreIntegrationTest, TestFieldsNotInFieldMaskAreIgnored) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(
      document.Set(MapFieldValue{{"desc", FieldValue::String("NewDescription")},
                                 {"owner", FieldValue::String("Sebastian")}},
                   SetOptions::MergeFields({"desc"})));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("NewDescription")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Jonny")},
                            {"email", FieldValue::String("abc@xyz.com")}})}}));
}

TEST_F(FirestoreIntegrationTest, TestFieldDeletesNotInFieldMaskAreIgnored) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(
      document.Set(MapFieldValue{{"desc", FieldValue::String("NewDescription")},
                                 {"owner", FieldValue::Delete()}},
                   SetOptions::MergeFields({"desc"})));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("NewDescription")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Jonny")},
                            {"email", FieldValue::String("abc@xyz.com")}})}}));
}

TEST_F(FirestoreIntegrationTest, TestFieldTransformsNotInFieldMaskAreIgnored) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(
      document.Set(MapFieldValue{{"desc", FieldValue::String("NewDescription")},
                                 {"owner", FieldValue::ServerTimestamp()}},
                   SetOptions::MergeFields({"desc"})));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("NewDescription")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Jonny")},
                            {"email", FieldValue::String("abc@xyz.com")}})}}));
}

TEST_F(FirestoreIntegrationTest, TestCanSetEmptyFieldMask) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(document.Set(
      MapFieldValue{{"desc", FieldValue::String("NewDescription")}},
      SetOptions::MergeFields({})));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("Description")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Jonny")},
                            {"email", FieldValue::String("abc@xyz.com")}})}}));
}

TEST_F(FirestoreIntegrationTest, TestCanSpecifyFieldsMultipleTimesInFieldMask) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(document.Set(
      MapFieldValue{
          {"desc", FieldValue::String("NewDescription")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                            {"email", FieldValue::String("new@new.com")}})}},
      SetOptions::MergeFields({"owner.name", "owner", "owner"})));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("Description")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                            {"email", FieldValue::String("new@new.com")}})}}));
}

TEST_F(FirestoreIntegrationTest, TestCanDeleteAFieldWithAnUpdate) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Jonny")},
                        {"email", FieldValue::String("abc@xyz.com")}})}}));
  Await(document.Update(MapFieldValue{{"owner.email", FieldValue::Delete()}}));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{
                  {"desc", FieldValue::String("Description")},
                  {"owner",
                   FieldValue::Map({{"name", FieldValue::String("Jonny")}})}}));
}

TEST_F(FirestoreIntegrationTest, TestCanUpdateFieldsWithDots) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{{"a.b", FieldValue::String("old")},
                                   {"c.d", FieldValue::String("old")},
                                   {"e.f", FieldValue::String("old")}}));
  Await(document.Update({{FieldPath{"a.b"}, FieldValue::String("new")}}));
  Await(document.Update({{FieldPath{"c.d"}, FieldValue::String("new")}}));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a.b", FieldValue::String("new")},
                                        {"c.d", FieldValue::String("new")},
                                        {"e.f", FieldValue::String("old")}}));
}

TEST_F(FirestoreIntegrationTest, TestCanUpdateNestedFields) {
  DocumentReference document = Collection("rooms").Document("eros");
  Await(document.Set(MapFieldValue{
      {"a", FieldValue::Map({{"b", FieldValue::String("old")}})},
      {"c", FieldValue::Map({{"d", FieldValue::String("old")}})},
      {"e", FieldValue::Map({{"f", FieldValue::String("old")}})}}));
  Await(document.Update({{"a.b", FieldValue::String("new")}}));
  Await(document.Update({{"c.d", FieldValue::String("new")}}));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{
                  {"a", FieldValue::Map({{"b", FieldValue::String("new")}})},
                  {"c", FieldValue::Map({{"d", FieldValue::String("new")}})},
                  {"e", FieldValue::Map({{"f", FieldValue::String("old")}})}}));
}

// Verify that multiple deletes in a single update call work.
// https://github.com/firebase/quickstart-unity/issues/882
TEST_F(FirestoreIntegrationTest, TestCanUpdateFieldsWithMultipleDeletes) {
  DocumentReference document = Collection("rooms").Document();
  Await(document.Set(MapFieldValue{{"key1", FieldValue::String("value1")},
                                   {"key2", FieldValue::String("value2")},
                                   {"key3", FieldValue::String("value3")},
                                   {"key4", FieldValue::String("value4")},
                                   {"key5", FieldValue::String("value5")}}));
  Await(document.Update({{FieldPath{"key1"}, FieldValue::Delete()},
                         {FieldPath{"key3"}, FieldValue::Delete()},
                         {FieldPath{"key5"}, FieldValue::Delete()}}));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(), ContainerEq(MapFieldValue{
                                      {"key2", FieldValue::String("value2")},
                                      {"key4", FieldValue::String("value4")}}));
}

TEST_F(FirestoreIntegrationTest, TestDeleteDocument) {
  DocumentReference document = Collection("rooms").Document("eros");
  WriteDocument(document, MapFieldValue{{"value", FieldValue::String("bar")}});
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"value", FieldValue::String("bar")}}));

  Await(document.Delete());
  snapshot = ReadDocument(document);
  EXPECT_FALSE(snapshot.exists());
}

TEST_F(FirestoreIntegrationTest, TestCannotUpdateNonexistentDocument) {
  DocumentReference document = Collection("rooms").Document();
  Future<void> future =
      document.Update(MapFieldValue{{"owner", FieldValue::String("abc")}});
  Await(future);
  EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
  EXPECT_EQ(Error::kErrorNotFound, future.error());
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_FALSE(snapshot.exists());
}

TEST_F(FirestoreIntegrationTest, TestCanRetrieveNonexistentDocument) {
  DocumentReference document = Collection("rooms").Document();
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_FALSE(snapshot.exists());

  TestEventListener<DocumentSnapshot> listener{"for document"};
  ListenerRegistration registration = listener.AttachTo(&document);
  Await(listener);
  EXPECT_EQ(Error::kErrorOk, listener.first_error_code());
  EXPECT_FALSE(listener.last_result().exists());
  registration.Remove();
}

TEST_F(FirestoreIntegrationTest,
       TestAddingToACollectionYieldsTheCorrectDocumentReference) {
  DocumentReference document = Collection("rooms").Document();
  Await(document.Set(MapFieldValue{{"foo", FieldValue::Double(1.0)}}));
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"foo", FieldValue::Double(1.0)}}));
}

TEST_F(FirestoreIntegrationTest,
       TestSnapshotsInSyncListenerFiresAfterListenersInSync) {
  class TestData {
   public:
    void AddEvent(const std::string& event) {
      MutexLock lock(mutex_);
      events_.push_back(event);
    }

    int GetEventCount() const {
      MutexLock lock(mutex_);
      return events_.size();
    }

    void ClearEvents() {
      MutexLock lock(mutex_);
      events_.clear();
    }

    void WaitForEventCount(const std::string& event, int expected_count) {
      while (true) {
        if (GetEventCount(event) >= expected_count) {
          break;
        }
      }
    }

    int GetEventCount(const std::string& event) const {
      MutexLock lock(mutex_);
      return std::count_if(events_.begin(), events_.end(),
                           [&event](const std::string& current_event) {
                             return current_event == event;
                           });
    }

    std::vector<std::string> GetEvents() const {
      MutexLock lock(mutex_);
      return events_;
    }

   private:
    mutable Mutex mutex_;
    std::vector<std::string> events_;
  };

  TestData test_data;

  DocumentReference document = Collection("rooms").Document();
  Await(document.Set(MapFieldValue{{"foo", FieldValue::Double(1.0)}}));

  class SnapshotTestEventListener : public TestEventListener<DocumentSnapshot> {
   public:
    SnapshotTestEventListener(std::string name, TestData& test_data)
        : TestEventListener(std::move(name)), test_data_(test_data) {}

    void OnEvent(const DocumentSnapshot& value,
                 Error error_code,
                 const std::string& error_message) override {
      TestEventListener::OnEvent(value, error_code, error_message);
      test_data_.AddEvent("doc");
    }

   private:
    TestData& test_data_;
  };
  SnapshotTestEventListener listener{"doc", test_data};
  ListenerRegistration doc_registration = listener.AttachTo(&document);
  // Wait for the initial event from the backend so that we know we'll get
  // exactly one snapshot event for our local write below.
  Await(listener);
  EXPECT_EQ(1, test_data.GetEventCount());
  test_data.ClearEvents();

  ListenerRegistration sync_registration =
      TestFirestore()->AddSnapshotsInSyncListener(
          [&test_data] { test_data.AddEvent("snapshots-in-sync"); });

  Await(document.Set(MapFieldValue{{"foo", FieldValue::Double(3.0)}}));
  // Wait for the snapshots-in-sync listener to fire afterwards.
  test_data.WaitForEventCount("snapshots-in-sync", 2);

  // We should have an initial snapshots-in-sync event, then a snapshot event
  // for set(), then another event to indicate we're in sync again.
  EXPECT_EQ(test_data.GetEvents(),
            std::vector<std::string>(
                {"snapshots-in-sync", "doc", "snapshots-in-sync"}));
  doc_registration.Remove();
  sync_registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestQueriesAreValidatedOnClient) {
  // NOTE: Failure cases are validated in ValidationTest.
  CollectionReference collection = Collection();
  Query query =
      collection.WhereGreaterThanOrEqualTo("x", FieldValue::Integer(32));
  // Same inequality field works;
  query.WhereLessThanOrEqualTo("x", FieldValue::String("cat"));
  // Equality on different field works;
  query.WhereEqualTo("y", FieldValue::String("cat"));
  // Array contains on different field works;
  query.WhereArrayContains("y", FieldValue::String("cat"));

  // Ordering by inequality field succeeds.
  query.OrderBy("x");
  collection.OrderBy("x").WhereGreaterThanOrEqualTo("x",
                                                    FieldValue::Integer(32));

  // inequality same as first order by works
  query.OrderBy("x").OrderBy("y");
  collection.OrderBy("x").OrderBy("y").WhereGreaterThanOrEqualTo(
      "x", FieldValue::Integer(32));
  collection.OrderBy("x", Query::Direction::kDescending)
      .WhereEqualTo("y", FieldValue::String("true"));

  // Equality different than orderBy works
  collection.OrderBy("x").WhereEqualTo("y", FieldValue::String("cat"));
  // Array contains different than orderBy works
  collection.OrderBy("x").WhereArrayContains("y", FieldValue::String("cat"));
}

// The test harness will generate Java JUnit test regardless whether this is
// inside a #if or not. So we move #if inside instead of enclose the whole case.
TEST_F(FirestoreIntegrationTest, TestListenCanBeCalledMultipleTimes) {
  class TestData {
   public:
    void SetDocumentSnapshot(const DocumentSnapshot& document_snapshot) {
      MutexLock lock(mutex_);
      document_snapshot_ = document_snapshot;
      is_document_snapshot_set_ = true;
    }

    DocumentSnapshot WaitForDocumentSnapshot() {
      while (true) {
        MutexLock lock(mutex_);
        if (is_document_snapshot_set_) {
          return document_snapshot_;
        }
      }
    }

   private:
    Mutex mutex_;
    DocumentSnapshot document_snapshot_;
    bool is_document_snapshot_set_ = false;
  };

  DocumentReference document = Collection("collection").Document();
  WriteDocument(document, MapFieldValue{{"foo", FieldValue::String("bar")}});
  TestData test_data;
  document.AddSnapshotListener(
      [&document, &test_data](const DocumentSnapshot& snapshot,
                              Error error_code,
                              const std::string& error_message) {
        EXPECT_EQ(Error::kErrorOk, error_code);
        EXPECT_EQ(std::string(), error_message);
        document.AddSnapshotListener(
            [&test_data](const DocumentSnapshot& snapshot, Error error_code,
                         const std::string& error_message) {
              EXPECT_EQ(Error::kErrorOk, error_code);
              EXPECT_EQ(std::string(), error_message);
              test_data.SetDocumentSnapshot(snapshot);
            });
      });

  DocumentSnapshot snapshot1 = test_data.WaitForDocumentSnapshot();
  EXPECT_TRUE(snapshot1.exists());
  EXPECT_THAT(snapshot1.GetData(),
              ContainerEq(MapFieldValue{{"foo", FieldValue::String("bar")}}));
}

TEST_F(FirestoreIntegrationTest, TestDocumentSnapshotEventsNonExistent) {
  DocumentReference document = Collection("rooms").Document();
  TestEventListener<DocumentSnapshot> listener("TestNonExistent");
  ListenerRegistration registration =
      listener.AttachTo(&document, MetadataChanges::kInclude);
  Await(listener);
  EXPECT_EQ(1, listener.event_count());
  EXPECT_EQ(Error::kErrorOk, listener.first_error_code());
  EXPECT_FALSE(listener.last_result().exists());
  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestDocumentSnapshotEventsForAdd) {
  DocumentReference document = Collection("rooms").Document();
  TestEventListener<DocumentSnapshot> listener("TestForAdd");
  ListenerRegistration registration =
      listener.AttachTo(&document, MetadataChanges::kInclude);
  Await(listener);
  EXPECT_FALSE(listener.last_result().exists());

  WriteDocument(document, MapFieldValue{{"a", FieldValue::Double(1.0)}});
  Await(listener, 3);
  DocumentSnapshot snapshot = listener.last_result(1);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());
  snapshot = listener.last_result();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());

  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestDocumentSnapshotEventsForChange) {
  CollectionReference collection =
      Collection(std::map<std::string, MapFieldValue>{
          {"doc", MapFieldValue{{"a", FieldValue::Double(1.0)}}}});
  DocumentReference document = collection.Document("doc");
  TestEventListener<DocumentSnapshot> listener("TestForChange");
  ListenerRegistration registration =
      listener.AttachTo(&document, MetadataChanges::kInclude);
  Await(listener);
  DocumentSnapshot snapshot = listener.last_result();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_FALSE(snapshot.metadata().is_from_cache());

  UpdateDocument(document, MapFieldValue{{"a", FieldValue::Double(2.0)}});
  Await(listener, 3);
  snapshot = listener.last_result(1);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(2.0)}}));
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  snapshot = listener.last_result();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(2.0)}}));
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_FALSE(snapshot.metadata().is_from_cache());

  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestDocumentSnapshotEventsForDelete) {
  CollectionReference collection =
      Collection(std::map<std::string, MapFieldValue>{
          {"doc", MapFieldValue{{"a", FieldValue::Double(1.0)}}}});
  DocumentReference document = collection.Document("doc");
  TestEventListener<DocumentSnapshot> listener("TestForDelete");
  ListenerRegistration registration =
      listener.AttachTo(&document, MetadataChanges::kInclude);
  Await(listener, 1);
  DocumentSnapshot snapshot = listener.last_result();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_FALSE(snapshot.metadata().is_from_cache());

  DeleteDocument(document);
  Await(listener, 2);
  snapshot = listener.last_result();
  EXPECT_FALSE(snapshot.exists());

  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestDocumentSnapshotErrorReporting) {
  DocumentReference document = Collection("col").Document("__badpath__");
  TestEventListener<DocumentSnapshot> listener("TestBadPath");
  ListenerRegistration registration =
      listener.AttachTo(&document, MetadataChanges::kInclude);
  Await(listener);
  EXPECT_EQ(1, listener.event_count());
  EXPECT_EQ(Error::kErrorInvalidArgument, listener.first_error_code());
  EXPECT_THAT(listener.first_error_message(), HasSubstr("__badpath__"));
  EXPECT_FALSE(listener.last_result().exists());
  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestQuerySnapshotEventsForAdd) {
  CollectionReference collection = Collection();
  DocumentReference document = collection.Document();
  TestEventListener<QuerySnapshot> listener("TestForCollectionAdd");
  ListenerRegistration registration =
      listener.AttachTo(&collection, MetadataChanges::kInclude);
  Await(listener);
  EXPECT_EQ(0, listener.last_result().size());

  WriteDocument(document, MapFieldValue{{"a", FieldValue::Double(1.0)}});
  Await(listener, 3);
  QuerySnapshot snapshot = listener.last_result(1);
  ASSERT_EQ(1, snapshot.size());
  EXPECT_TRUE(snapshot.documents()[0].exists());
  EXPECT_THAT(snapshot.documents()[0].GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());
  snapshot = listener.last_result();
  ASSERT_EQ(1, snapshot.size());
  EXPECT_TRUE(snapshot.documents()[0].exists());
  EXPECT_THAT(snapshot.documents()[0].GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());

  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestQuerySnapshotEventsForChange) {
  CollectionReference collection =
      Collection(std::map<std::string, MapFieldValue>{
          {"doc", MapFieldValue{{"a", FieldValue::Double(1.0)}}}});
  DocumentReference document = collection.Document("doc");
  TestEventListener<QuerySnapshot> listener("TestForCollectionChange");
  ListenerRegistration registration =
      listener.AttachTo(&collection, MetadataChanges::kInclude);
  Await(listener);
  QuerySnapshot snapshot = listener.last_result();
  ASSERT_EQ(1, snapshot.size());
  EXPECT_TRUE(snapshot.documents()[0].exists());
  EXPECT_THAT(snapshot.documents()[0].GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());

  WriteDocument(document, MapFieldValue{{"a", FieldValue::Double(2.0)}});
  Await(listener, 3);
  snapshot = listener.last_result(1);
  ASSERT_EQ(1, snapshot.size());
  EXPECT_TRUE(snapshot.documents()[0].exists());
  EXPECT_THAT(snapshot.documents()[0].GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(2.0)}}));
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());
  snapshot = listener.last_result();
  ASSERT_EQ(1, snapshot.size());
  EXPECT_TRUE(snapshot.documents()[0].exists());
  EXPECT_THAT(snapshot.documents()[0].GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(2.0)}}));
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());

  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestQuerySnapshotEventsForDelete) {
  CollectionReference collection =
      Collection(std::map<std::string, MapFieldValue>{
          {"doc", MapFieldValue{{"a", FieldValue::Double(1.0)}}}});
  DocumentReference document = collection.Document("doc");
  TestEventListener<QuerySnapshot> listener("TestForQueryDelete");
  ListenerRegistration registration =
      listener.AttachTo(&collection, MetadataChanges::kInclude);
  Await(listener);
  QuerySnapshot snapshot = listener.last_result();
  ASSERT_EQ(1, snapshot.size());
  EXPECT_TRUE(snapshot.documents()[0].exists());
  EXPECT_THAT(snapshot.documents()[0].GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());

  DeleteDocument(document);
  Await(listener, 2);
  snapshot = listener.last_result();
  EXPECT_EQ(0, snapshot.size());

  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestQuerySnapshotErrorReporting) {
  CollectionReference collection =
      Collection("a").Document("__badpath__").Collection("b");
  TestEventListener<QuerySnapshot> listener("TestBadPath");
  ListenerRegistration registration =
      listener.AttachTo(&collection, MetadataChanges::kInclude);
  Await(listener);
  EXPECT_EQ(1, listener.event_count());
  EXPECT_EQ(Error::kErrorInvalidArgument, listener.first_error_code());
  EXPECT_THAT(listener.first_error_message(), HasSubstr("__badpath__"));
  EXPECT_TRUE(listener.last_result().empty());
  registration.Remove();
}

TEST_F(FirestoreIntegrationTest,
       TestMetadataOnlyChangesAreNotFiredWhenNoOptionsProvided) {
  DocumentReference document = Collection().Document();
  TestEventListener<DocumentSnapshot> listener("TestForNoMetadataOnlyChanges");
  ListenerRegistration registration = listener.AttachTo(&document);
  WriteDocument(document, MapFieldValue{{"a", FieldValue::Double(1.0)}});
  Await(listener);
  EXPECT_TRUE(listener.last_result().exists());
  EXPECT_THAT(listener.last_result().GetData(),
              ContainerEq(MapFieldValue{{"a", FieldValue::Double(1.0)}}));
  WriteDocument(document, MapFieldValue{{"b", FieldValue::Double(1.0)}});
  Await(listener);
  EXPECT_TRUE(listener.last_result().exists());
  EXPECT_THAT(listener.last_result().GetData(),
              ContainerEq(MapFieldValue{{"b", FieldValue::Double(1.0)}}));
  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, TestDocumentReferenceExposesFirestore) {
  Firestore* db = TestFirestore();
  // EXPECT_EQ(db, db->Document("foo/bar").firestore());
  // TODO(varconst): use the commented out check above.
  // Currently, integration tests create their own Firestore instances that
  // aren't registered in the main cache. Because of that, Firestore objects
  // will lazily create a new Firestore instance upon the first access. This
  // doesn't affect production code, only tests.
  // Also, the logic in `util_main.h` can be modified to make sure that
  // `TestFirestore()` doesn't create a new Firestore instance if there isn't
  // one already.
  EXPECT_NE(nullptr, db->Document("foo/bar").firestore());
}

TEST_F(FirestoreIntegrationTest, TestCollectionReferenceExposesFirestore) {
  Firestore* db = TestFirestore();
  // EXPECT_EQ(db, db->Collection("foo").firestore());
  EXPECT_NE(nullptr, db->Collection("foo").firestore());
}

TEST_F(FirestoreIntegrationTest, TestQueryExposesFirestore) {
  Firestore* db = TestFirestore();
  // EXPECT_EQ(db, db->Collection("foo").Limit(5).firestore());
  EXPECT_NE(nullptr, db->Collection("foo").Limit(5).firestore());
}

TEST_F(FirestoreIntegrationTest, TestDocumentReferenceEquality) {
  Firestore* db = TestFirestore();
  DocumentReference document = db->Document("foo/bar");
  EXPECT_EQ(document, db->Document("foo/bar"));
  EXPECT_EQ(document, document.Collection("blah").Parent());

  EXPECT_NE(document, db->Document("foo/BAR"));

  Firestore* another_db = TestFirestore("another");
  EXPECT_NE(document, another_db->Document("foo/bar"));
}

TEST_F(FirestoreIntegrationTest, TestQueryReferenceEquality) {
  Firestore* db = TestFirestore();
  Query query = db->Collection("foo").OrderBy("bar").WhereEqualTo(
      "baz", FieldValue::Integer(42));
  Query query2 = db->Collection("foo").OrderBy("bar").WhereEqualTo(
      "baz", FieldValue::Integer(42));
  EXPECT_TRUE(query == query2);

  Query query3 = db->Collection("foo").OrderBy("BAR").WhereEqualTo(
      "baz", FieldValue::Integer(42));
  EXPECT_FALSE(query == query3);

  // PORT_NOTE: Right now there is no way to create another Firestore in test.
  // So we skip the testing of two queries with different Firestore instance.
}

TEST_F(FirestoreIntegrationTest, TestCanTraverseCollectionsAndDocuments) {
  Firestore* db = TestFirestore();

  // doc path from root Firestore.
  EXPECT_EQ("a/b/c/d", db->Document("a/b/c/d").path());

  // collection path from root Firestore.
  EXPECT_EQ("a/b/c/d", db->Collection("a/b/c").Document("d").path());

  // doc path from CollectionReference.
  EXPECT_EQ("a/b/c/d", db->Collection("a").Document("b/c/d").path());

  // collection path from DocumentReference.
  EXPECT_EQ("a/b/c/d/e", db->Document("a/b").Collection("c/d/e").path());
}

TEST_F(FirestoreIntegrationTest, TestCanTraverseCollectionAndDocumentParents) {
  Firestore* db = TestFirestore();
  CollectionReference collection = db->Collection("a/b/c");
  EXPECT_EQ("a/b/c", collection.path());

  DocumentReference doc = collection.Parent();
  EXPECT_EQ("a/b", doc.path());

  collection = doc.Parent();
  EXPECT_EQ("a", collection.path());

  DocumentReference invalidDoc = collection.Parent();
  EXPECT_FALSE(invalidDoc.is_valid());
}

TEST_F(FirestoreIntegrationTest, TestCollectionId) {
  EXPECT_EQ("foo", TestFirestore()->Collection("foo").id());
  EXPECT_EQ("baz", TestFirestore()->Collection("foo/bar/baz").id());
}

TEST_F(FirestoreIntegrationTest, TestDocumentId) {
  EXPECT_EQ(TestFirestore()->Document("foo/bar").id(), "bar");
  EXPECT_EQ(TestFirestore()->Document("foo/bar/baz/qux").id(), "qux");
}

TEST_F(FirestoreIntegrationTest, TestCanQueueWritesWhileOffline) {
  // Arrange
  DocumentReference document = Collection("rooms").Document("eros");

  // Act
  Await(TestFirestore()->DisableNetwork());
  Future<void> future = document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                        {"email", FieldValue::String("abc@xyz.com")}})}});
  EXPECT_EQ(FutureStatus::kFutureStatusPending, future.status());
  Await(TestFirestore()->EnableNetwork());
  Await(future);

  // Assert
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("Description")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                            {"email", FieldValue::String("abc@xyz.com")}})}}));
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
}

TEST_F(FirestoreIntegrationTest, TestCanGetDocumentsWhileOffline) {
  DocumentReference document = Collection("rooms").Document();
  Await(TestFirestore()->DisableNetwork());
  Future<DocumentSnapshot> future = document.Get();
  Await(future);
  EXPECT_EQ(Error::kErrorUnavailable, future.error());

  // Write the document to the local cache.
  Future<void> pending_write = document.Set(MapFieldValue{
      {"desc", FieldValue::String("Description")},
      {"owner",
       FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                        {"email", FieldValue::String("abc@xyz.com")}})}});

  // The network is offline and we return a cached result.
  DocumentSnapshot snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("Description")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                            {"email", FieldValue::String("abc@xyz.com")}})}}));
  EXPECT_TRUE(snapshot.metadata().is_from_cache());

  // Enable the network and fetch the document again.
  Await(TestFirestore()->EnableNetwork());
  Await(pending_write);
  snapshot = ReadDocument(document);
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      ContainerEq(MapFieldValue{
          {"desc", FieldValue::String("Description")},
          {"owner",
           FieldValue::Map({{"name", FieldValue::String("Sebastian")},
                            {"email", FieldValue::String("abc@xyz.com")}})}}));
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
}

// Will not port the following two cases:
//   TestWriteStreamReconnectsAfterIdle and
//   TestWatchStreamReconnectsAfterIdle,
// both of which requires manipulating with DispatchQueue which is not exposed
// as a public API.
// Also, these tests exercise a particular part of SDK (streams), they are
// really unit tests that have to be run in integration tests setup. The
// existing Objective-C and Android tests cover these cases fairly well.

TEST_F(FirestoreIntegrationTest, TestCanDisableAndEnableNetworking) {
  // There's not currently a way to check if networking is in fact disabled,
  // so for now just test that the method is well-behaved and doesn't throw.
  Firestore* db = TestFirestore();
  Await(db->EnableNetwork());
  Await(db->EnableNetwork());
  Await(db->DisableNetwork());
  Await(db->DisableNetwork());
  Await(db->EnableNetwork());
}

// TODO(varconst): split this test.
TEST_F(FirestoreIntegrationTest, TestToString) {
  Settings settings;
  settings.set_host("foo.bar");
  settings.set_ssl_enabled(false);
  EXPECT_EQ(
      "Settings(host='foo.bar', is_ssl_enabled=false, "
      "is_persistence_enabled=true, cache_size_bytes=104857600)",
      settings.ToString());

  CollectionReference collection = Collection("rooms");
  DocumentReference reference = collection.Document("eros");
  // Note: because the map is unordered, it's hard to check the case where a map
  // has more than one element.
  Await(reference.Set({
      {"owner", FieldValue::String("Jonny")},
  }));
  EXPECT_EQ(std::string("DocumentReference(") + collection.id() + "/eros)",
            reference.ToString());

  DocumentSnapshot doc = ReadDocument(reference);
  EXPECT_EQ(
      "DocumentSnapshot(id=eros, "
      "metadata=SnapshotMetadata{has_pending_writes=false, "
      "is_from_cache=false}, doc={owner: 'Jonny'})",
      doc.ToString());
}

// TODO(wuandy): Enable this for other platforms when they can handle
// exceptions.
#if defined(__ANDROID__) && FIRESTORE_HAVE_EXCEPTIONS
TEST_F(FirestoreIntegrationTest, ClientCallsAfterTerminateFails) {
  EXPECT_THAT(TestFirestore()->Terminate(), FutureSucceeds());
  EXPECT_THROW(Await(TestFirestore()->DisableNetwork()), std::logic_error);
}

TEST_F(FirestoreIntegrationTest, NewOperationThrowsAfterFirestoreTerminate) {
  auto instance = TestFirestore();
  DocumentReference reference = TestFirestore()->Document("abc/123");
  Await(reference.Set({{"Field", FieldValue::Integer(100)}}));

  EXPECT_THAT(instance->Terminate(), FutureSucceeds());

  EXPECT_THROW(Await(reference.Get()), std::logic_error);
  EXPECT_THROW(Await(reference.Update({{"Field", FieldValue::Integer(1)}})),
               std::logic_error);
  EXPECT_THROW(Await(reference.Set({{"Field", FieldValue::Integer(1)}})),
               std::logic_error);
  EXPECT_THROW(Await(instance->batch()
                         .Set(reference, {{"Field", FieldValue::Integer(1)}})
                         .Commit()),
               std::logic_error);
  EXPECT_THROW(Await(instance->RunTransaction(
                   [reference](Transaction& transaction,
                               std::string& error_message) -> Error {
                     Error error = Error::kErrorOk;
                     transaction.Get(reference, &error, &error_message);
                     return error;
                   })),
               std::logic_error);
}

TEST_F(FirestoreIntegrationTest, TerminateCanBeCalledMultipleTimes) {
  auto instance = TestFirestore();
  DocumentReference reference = instance->Document("abc/123");
  Await(reference.Set({{"Field", FieldValue::Integer(100)}}));

  EXPECT_THAT(instance->Terminate(), FutureSucceeds());

  EXPECT_THROW(Await(reference.Get()), std::logic_error);

  // Calling a second time should go through and change nothing.
  EXPECT_THAT(instance->Terminate(), FutureSucceeds());

  EXPECT_THROW(Await(reference.Update({{"Field", FieldValue::Integer(1)}})),
               std::logic_error);
}
#endif  // defined(__ANDROID__) && FIRESTORE_HAVE_EXCEPTIONS

TEST_F(FirestoreIntegrationTest, MaintainsPersistenceAfterRestarting) {
  Firestore* db = TestFirestore();
  App* app = db->app();
  DocumentReference doc = db->Collection("col1").Document("doc1");
  auto path = doc.path();
  Await(doc.Set({{"foo", FieldValue::String("bar")}}));
  DeleteFirestore(db);
  DeleteApp(app);

  DocumentReference doc_2 = TestFirestore()->Document(path);
  auto snap = Await(doc_2.Get());
  EXPECT_TRUE(snap->exists());
}

TEST_F(FirestoreIntegrationTest, RestartFirestoreLeadsToNewInstance) {
  // Get App and Settings objects to use in the test.
  Firestore* db_template = TestFirestore("restart_firestore_new_instance_test");
  App* app = db_template->app();
  Settings settings = db_template->settings();
  DeleteFirestore(db_template);

  // Verify that GetInstance() returns the same instance when specified the same
  // App.
  InitResult init_result;
  Firestore* db1 = Firestore::GetInstance(app, &init_result);
  ASSERT_EQ(kInitResultSuccess, init_result);
  Firestore* db1_copy = Firestore::GetInstance(app, &init_result);
  ASSERT_EQ(kInitResultSuccess, init_result);
  EXPECT_EQ(db1, db1_copy);

  // Create a document that we can use for verification later.
  db1->set_settings(settings);
  DocumentReference doc1 = db1->Collection("abc").Document();
  const std::string doc_path = doc1.path();
  EXPECT_THAT(doc1.Set({{"foo", FieldValue::String("bar")}}), FutureSucceeds());

  // Terminate `db1` so that it will be removed from the instance cache.
  EXPECT_THAT(db1->Terminate(), FutureSucceeds());

  // Verify that GetInstance() returns a new instance since the old instance has
  // been terminated.
  Firestore* db2 = Firestore::GetInstance(app, &init_result);
  ASSERT_EQ(kInitResultSuccess, init_result);
  EXPECT_NE(db1, db2);

  // Verify that the new instance points to the same database by verifying that
  // the document created with the old instance exists in the new instance.
  DocumentReference doc2 = db2->Document(doc_path);
  const DocumentSnapshot* snapshot2 = Await(doc2.Get(Source::kCache));
  ASSERT_NE(snapshot2, nullptr);
  EXPECT_TRUE(snapshot2->exists());
  EXPECT_THAT(snapshot2->GetData(),
              ContainerEq(MapFieldValue{{"foo", FieldValue::String("bar")}}));

  delete db2;
  delete db1;
}

TEST_F(FirestoreIntegrationTest, CanStopListeningAfterTerminate) {
  auto instance = TestFirestore();
  DocumentReference reference = instance->Document("abc/123");
  EventAccumulator<DocumentSnapshot> accumulator;
  ListenerRegistration registration =
      accumulator.listener()->AttachTo(&reference);

  accumulator.Await();
  EXPECT_THAT(instance->Terminate(), FutureSucceeds());

  // This should proceed without error.
  registration.Remove();
  // Multiple calls should proceed as effectively a no-op.
  registration.Remove();
}

TEST_F(FirestoreIntegrationTest, WaitForPendingWritesResolves) {
  DocumentReference document = Collection("abc").Document("123");

  Await(TestFirestore()->DisableNetwork());
  Future<void> await_pending_writes_1 = TestFirestore()->WaitForPendingWrites();
  Future<void> pending_writes =
      document.Set(MapFieldValue{{"desc", FieldValue::String("Description")}});
  Future<void> await_pending_writes_2 = TestFirestore()->WaitForPendingWrites();

  // `await_pending_writes_1` resolves immediately because there are no pending
  // writes at the time it is created.
  Await(await_pending_writes_1);
  EXPECT_EQ(await_pending_writes_1.status(),
            FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(pending_writes.status(), FutureStatus::kFutureStatusPending);
  EXPECT_EQ(await_pending_writes_2.status(),
            FutureStatus::kFutureStatusPending);

  TestFirestore()->EnableNetwork();
  Await(await_pending_writes_2);
  EXPECT_EQ(await_pending_writes_2.status(),
            FutureStatus::kFutureStatusComplete);
}

// TODO(wuandy): This test requires to create underlying firestore instance with
// a MockCredentialProvider first.
// TEST_F(FirestoreIntegrationTest, WaitForPendingWritesFailsWhenUserChanges) {}

TEST_F(FirestoreIntegrationTest,
       WaitForPendingWritesResolvesWhenOfflineIfThereIsNoPending) {
  Await(TestFirestore()->DisableNetwork());
  Future<void> await_pending_writes = TestFirestore()->WaitForPendingWrites();

  // `await_pending_writes` resolves immediately because there are no pending
  // writes at the time it is created.
  Await(await_pending_writes);
  EXPECT_EQ(await_pending_writes.status(), FutureStatus::kFutureStatusComplete);
}

TEST_F(FirestoreIntegrationTest, CanClearPersistenceTestHarnessVerification) {
  // Verify that TestFirestore(), DeleteFirestore(), and DeleteApp() behave how
  // we expect; otherwise, the tests for ClearPersistence() could yield false
  // positives.
  Firestore* db = TestFirestore();
  App* app = db->app();
  const std::string app_name = app->name();

  DocumentReference document = db->Collection("a").Document();
  const std::string path = document.path();
  WriteDocument(document, MapFieldValue{{"foo", FieldValue::Integer(42)}});
  DeleteFirestore(db);
  DeleteApp(app);

  Firestore* db_2 = TestFirestore(app_name);
  DocumentReference document_2 = db_2->Document(path);
  Future<DocumentSnapshot> get_future = document_2.Get(Source::kCache);
  DocumentSnapshot snapshot_2 = *Await(get_future);
  EXPECT_TRUE(snapshot_2.exists());
  EXPECT_THAT(snapshot_2.GetData(),
              ContainerEq(MapFieldValue{{"foo", FieldValue::Integer(42)}}));
}

TEST_F(FirestoreIntegrationTest, CanClearPersistenceAfterRestarting) {
  Firestore* db = TestFirestore();
  App* app = db->app();
  const std::string app_name = app->name();

  DocumentReference document = db->Collection("a").Document("b");
  const std::string path = document.path();
  WriteDocument(document, MapFieldValue{{"foo", FieldValue::Integer(42)}});

  // Call ClearPersistence(), but call Terminate() first because
  // ClearPersistence() requires Firestore to be terminated.
  EXPECT_THAT(db->Terminate(), FutureSucceeds());
  EXPECT_THAT(db->ClearPersistence(), FutureSucceeds());
  // Call DeleteFirestore() to ensure that both the App and Firestore instances
  // are deleted, which emulates the way an end user would experience their
  // application being killed and later re-launched by the user.
  DeleteFirestore(db);
  DeleteApp(app);

  // We restart the app with the same name and options to check that the
  // previous instance's persistent storage is actually cleared after the
  // restart. Although calling TestFirestore() with no arguments here would do
  // the same thing, we explicitly specify an app_name to be clear that we want
  // a new Firestore instance for the same Firebase app.
  Firestore* db_2 = TestFirestore(app_name);
  DocumentReference document_2 = db_2->Document(path);
  Future<DocumentSnapshot> await_get = document_2.Get(Source::kCache);
  Await(await_get);
  EXPECT_EQ(await_get.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(await_get.error(), Error::kErrorUnavailable);
}

TEST_F(FirestoreIntegrationTest, CanClearPersistenceOnANewFirestoreInstance) {
  Firestore* db = TestFirestore();
  App* app = db->app();
  const std::string app_name = app->name();

  DocumentReference document = db->Collection("a").Document("b");
  const std::string path = document.path();
  WriteDocument(document, MapFieldValue{{"foo", FieldValue::Integer(42)}});

  // Call DeleteFirestore() to ensure that both the App and Firestore instances
  // are deleted, which emulates the way an end user would experience their
  // application being killed and later re-launched by the user.
  DeleteFirestore(db);
  DeleteApp(app);

  // We restart the app with the same name and options to check that the
  // previous instance's persistent storage is actually cleared after the
  // restart. Although calling TestFirestore() with no arguments here would do
  // the same thing, we explicitly specify an app_name to be clear that we want
  // a new Firestore instance for the same Firebase app.
  Firestore* db_2 = TestFirestore(app_name);
  EXPECT_THAT(db_2->ClearPersistence(), FutureSucceeds());
  DocumentReference document_2 = db_2->Document(path);
  Future<DocumentSnapshot> await_get = document_2.Get(Source::kCache);
  Await(await_get);
  EXPECT_EQ(await_get.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(await_get.error(), Error::kErrorUnavailable);
}

TEST_F(FirestoreIntegrationTest, ClearPersistenceWhileRunningFails) {
  // Call EnableNetwork() in order to ensure that Firestore is fully
  // initialized before clearing persistence. EnableNetwork() is chosen because
  // it is easy to call.
  Await(TestFirestore()->EnableNetwork());
  Future<void> await_clear_persistence = TestFirestore()->ClearPersistence();
  Await(await_clear_persistence);
  EXPECT_EQ(await_clear_persistence.status(),
            FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(await_clear_persistence.error(), Error::kErrorFailedPrecondition);
}

// Note: this test only exists in C++.
TEST_F(FirestoreIntegrationTest, DomainObjectsReferToSameFirestoreInstance) {
  EXPECT_EQ(TestFirestore(), TestFirestore()->Document("foo/bar").firestore());
  EXPECT_EQ(TestFirestore(), TestFirestore()->Collection("foo").firestore());
}

TEST_F(FirestoreIntegrationTest, AuthWorks) {
  SKIP_TEST_ON_QUICK_CHECK;
  // This app instance is managed by the text fixture.
  App* app = GetApp();
  EXPECT_NE(app, nullptr);

  InitResult init_result;
  auto auth = std::unique_ptr<Auth>(Auth::GetAuth(app, &init_result));
#if defined(__ANDROID__)
  if (init_result != kInitResultSuccess) {
    // On Android, it's possible for the Auth library built at head to be too
    // new for the version of Play Services available in the Android emulator.
    // In this case, Auth will fail to initialize. Meanwhile, there's no simple
    // way to detect if the Android app is running in an emulator running on
    // Forge. Consequently, just punt if Auth fails to initialize.
    LogWarning("Skipped AuthWorks test: Auth missing or failed to initialize");
    return;
  }
#else
  ASSERT_EQ(init_result, kInitResultSuccess);
#endif

  auto db =
      std::unique_ptr<Firestore>(Firestore::GetInstance(app, &init_result));
  EXPECT_EQ(init_result, kInitResultSuccess);

  // Performing a write will initialize Firestore's worker and get the current
  // user and token from Auth.
  DocumentReference doc = db->Collection(util::CreateAutoId()).Document();
  WriteDocument(doc, MapFieldValue{{"foo", FieldValue::Integer(42)}});

  // Signing in should trigger an AuthStateListener event.
  auto signin = auth->SignInAnonymously();
  Stopwatch stopwatch;
  Await(signin);
  stopwatch.stop();
  FailIfUnsuccessful("SignInAnonymously", signin, stopwatch);

  // Writing again will trigger another pull of the token.
  WriteDocument(doc, MapFieldValue{{"foo", FieldValue::Integer(43)}});
}

// This test is to ensure b/172986326 doesn't regress.
// Note: this test only exists in C++.
TEST_F(FirestoreIntegrationTest, FirestoreCanBeDeletedFromTransactionAsync) {
  Firestore* db = TestFirestore();
  DisownFirestore(db);

  auto future = db->RunTransaction(
      [](Transaction&, std::string&) { return Error::kErrorOk; });

  std::future<void> deletion;
  std::promise<void> callback_done_promise;
  auto callback_done = callback_done_promise.get_future();
  future.AddOnCompletion([&](const Future<void>&) mutable {
    deletion = std::async([db] { delete db; });
    callback_done_promise.set_value();
  });

  Await(future);
  callback_done.wait();
  deletion.wait();
}

// This test is to ensure b/172986326 doesn't regress.
// Note: this test only exists in C++.
TEST_F(FirestoreIntegrationTest, FirestoreCanBeDeletedFromTransaction) {
  Firestore* db = TestFirestore();
  DisownFirestore(db);

  auto future = db->RunTransaction(
      [](Transaction&, std::string&) { return Error::kErrorOk; });

  std::promise<void> callback_done_promise;
  auto callback_done = callback_done_promise.get_future();
  future.AddOnCompletion([&](const Future<void>&) mutable {
    delete db;
    callback_done_promise.set_value();
  });

  Await(future);
  callback_done.wait();
}

#if defined(__ANDROID__)
TEST_F(FirestoreAndroidIntegrationTest,
       CanDeleteFirestoreInstanceOnJavaMainThread) {
  jni::Env env;
  Firestore* db = TestFirestore();
  auto runnable = MakeJniRunnable(env, [db] { delete db; });

  jni::Local<jni::Task> task = runnable.RunOnMainThread(env);

  Await(env, task);
  EXPECT_TRUE(task.IsSuccessful(env));
  DisownFirestore(db);  // Avoid double-deletion of the `db`.
}
#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
