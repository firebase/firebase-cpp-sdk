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

#include <cmath>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "Firestore/core/src/util/firestore_exceptions.h"
#include "firebase/firestore.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/macros.h"
#include "firestore_integration_test.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/event_accumulator.h"
#include "util/future_test_util.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRValidationTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/ValidationTest.java
//
// PORT_NOTE: C++ exceptions are forbidden by the Google style guide. However,
// when building the Unity SDK, we build C++ with exceptions enabled which
// allows us to propagate C++ exceptions as C# exceptions. The exact exceptions
// being thrown are easier to check on the C++ layer, so these tests must be
// built with exceptions enabled.

namespace firebase {
namespace firestore {

#if FIRESTORE_HAVE_EXCEPTIONS

using testing::AnyOf;
using testing::HasSubstr;
using testing::Property;
using testing::StrEq;
using testing::Throws;

// Note: there is some issue with the `ThrowsMessage` matcher that prevents it
// from printing the actual exception message to the console.
#define EXPECT_ERROR(stmt, msg)                               \
  EXPECT_THAT([&] { stmt; }, Throws<std::exception>(Property( \
                                 &std::exception::what, StrEq(msg))));

#define EXPECT_ERROR_EITHER(stmt, msg1, msg2)  \
  EXPECT_THAT([&] { stmt; },                   \
              Throws<std::exception>(Property( \
                  &std::exception::what, AnyOf(StrEq(msg1), StrEq(msg2)))));

namespace {

enum class ErrorCase {
  kSettingsAfterUse,
  kSettingsDisableSsl,
  kFieldValueDeleteInSet,
  kFieldValueDeleteNested,
  kArrayUnionInQuery,
  kArrayRemoveInQuery,
  kQueryMissingOrderBy,
  kQueryOrderByTooManyArguments,
  kQueryInvalidBoundInteger,
  kQueryInvalidBoundWithSlash,
  kQueryDifferentInequalityFields,
  kQueryInequalityOrderByDifferentFields,
  kQueryMultipleArrayContains,
  kQueryStartBoundWithoutOrderBy,
  kQueryEndBoundWithoutOrderBy,
  kQueryDocumentIdEmpty,
  kQueryDocumentIdSlash,
  kQueryDocumentIdInteger,
  kQueryDocumentIdArrayContains
};

// Returns the exact error message used on a platform (in many cases, Android is
// different from other platforms because it wraps the actual Android SDK).
// TODO(b/171990785): Unify Android and C++ validation error messages.
std::string ErrorMessage(ErrorCase error_case) {
  switch (error_case) {
    case ErrorCase::kSettingsAfterUse:
#ifdef __ANDROID__
      return "FirebaseFirestore has already been started and its settings can "
             "no longer be changed. You can only call setFirestoreSettings() "
             "before calling any other methods on a FirebaseFirestore object.";
#else
      return "Firestore instance has already been started and its settings can "
             "no longer be changed. You can only set settings before calling "
             "any other methods on a Firestore instance.";
#endif

    case ErrorCase::kSettingsDisableSsl:
#ifdef __ANDROID__
      return "You can't set the 'sslEnabled' setting unless you also set a "
             "non-default 'host'.";
#else
      return "You can't set the 'sslEnabled' setting to false unless you also "
             "set a non-default 'host'.";
#endif

    case ErrorCase::kFieldValueDeleteInSet:
#ifdef __ANDROID__
      return "Invalid data. FieldValue.delete() can only be used with update() "
             "and set() with SetOptions.merge() (found in field foo)";
#else
      return "Invalid data. FieldValue::Delete() can only be used with "
             "Update() and Set() with merge == true (found in field foo)";
#endif

    case ErrorCase::kFieldValueDeleteNested:
#ifdef __ANDROID__
      return "Invalid data. FieldValue.delete() can only appear at the top "
             "level of your update data (found in field foo.bar)";
#else
      return "Invalid data. FieldValue::Delete() can only appear at the top "
             "level of your update data (found in field foo.bar)";
#endif

    case ErrorCase::kArrayUnionInQuery:
#ifdef __ANDROID__
      return "Invalid data. FieldValue.arrayUnion() can only be used with "
             "set() and update() (found in field test)";
#else
      // TODO(b/171990785): Note that `Update` and `Set` are reversed in this
      // message.
      return "Invalid data. FieldValue::ArrayUnion() can only be used with "
             "Update() and Set() (found in field test)";
#endif

    case ErrorCase::kArrayRemoveInQuery:
#ifdef __ANDROID__
      return "Invalid data. FieldValue.arrayRemove() can only be used with "
             "set() and update() (found in field test)";
#else
      // TODO(b/171990785): Note that `Update` and `Set` are reversed in this
      // message.
      return "Invalid data. FieldValue::ArrayRemove() can only be used with "
             "Update() and Set() (found in field test)";
#endif

    case ErrorCase::kQueryMissingOrderBy:
#ifdef __ANDROID__
      return "Invalid query. You are trying to start or end a query using a "
             "document for which the field 'sort' (used as the orderBy) does "
             "not exist.";
#else
      return "Invalid query. You are trying to start or end a query using a "
             "document for which the field 'sort' (used as the order by) does "
             "not exist.";
#endif

    case ErrorCase::kQueryOrderByTooManyArguments:
#ifdef __ANDROID__
      return "Too many arguments provided to startAt(). The number of "
             "arguments must be less than or equal to the number of orderBy() "
             "clauses.";
#else
      return "Invalid query. You are trying to start or end a query using more "
             "values than were specified in the order by.";
#endif

    case ErrorCase::kQueryInvalidBoundInteger:
#ifdef __ANDROID__
      return "Invalid query. Expected a string for document ID in startAt(), "
             "but got 1.";
#else
      return "Invalid query. Expected a string for the document ID.";
#endif

    case ErrorCase::kQueryInvalidBoundWithSlash:
#ifdef __ANDROID__
      return "Invalid query. When querying a collection and ordering by "
             "FieldPath.documentId(), the value passed to startAt() must be a "
             "plain document ID, but 'foo/bar' contains a slash.";
#else
      return "Invalid query. When querying a collection and ordering by "
             "document ID, you must pass a plain document ID, but 'foo/bar' "
             "contains a slash.";
#endif

    case ErrorCase::kQueryDifferentInequalityFields:
#ifdef __ANDROID__
      return "All where filters with an inequality (notEqualTo, notIn, "
             "lessThan, lessThanOrEqualTo, greaterThan, or "
             "greaterThanOrEqualTo) must be on the same field. But you have "
             "filters on 'x' and 'y'";
#else
      return "Invalid Query. All where filters with an inequality (notEqual, "
             "lessThan, lessThanOrEqual, greaterThan, or greaterThanOrEqual) "
             "must be on the same field. But you have inequality filters on "
             "'x' and 'y'";
#endif

    case ErrorCase::kQueryInequalityOrderByDifferentFields:
#ifdef __ANDROID__
      return "Invalid query. You have an inequality where filter "
             "(whereLessThan(), whereGreaterThan(), etc.) on field 'x' and so "
             "you must also have 'x' as your first orderBy() field, but your "
             "first orderBy() is currently on field 'y' instead.";
#else
      return "Invalid query. You have a where filter with an inequality "
             "(notEqual, lessThan, lessThanOrEqual, greaterThan, or "
             "greaterThanOrEqual) on field 'x' and so you must also use 'x' as "
             "your first queryOrderedBy field, but your first queryOrderedBy "
             "is currently on field 'y' instead.";
#endif

    case ErrorCase::kQueryMultipleArrayContains:
#ifdef __ANDROID__
      return "Invalid Query. You cannot use more than one 'array_contains' "
             "filter.";
#else
      return "Invalid Query. You cannot use more than one 'arrayContains' "
             "filter.";
#endif

    case ErrorCase::kQueryStartBoundWithoutOrderBy:
#ifdef __ANDROID__
      return "Invalid query. You must not call Query.startAt() or "
             "Query.startAfter() before calling Query.orderBy().";
#else
      return "Invalid query. You must not specify a starting point before "
             "specifying the order by.";
#endif

    case ErrorCase::kQueryEndBoundWithoutOrderBy:
#ifdef __ANDROID__
      return "Invalid query. You must not call Query.endAt() or "
             "Query.endBefore() before calling Query.orderBy().";
#else
      return "Invalid query. You must not specify an ending point before "
             "specifying the order by.";
#endif

    case ErrorCase::kQueryDocumentIdEmpty:
#ifdef __ANDROID__
      return "Invalid query. When querying with FieldPath.documentId() you "
             "must provide a valid document ID, but it was an empty string.";
#else
      return "Invalid query. When querying by document ID you must provide a "
             "valid document ID, but it was an empty string.";
#endif

    case ErrorCase::kQueryDocumentIdSlash:
#ifdef __ANDROID__
      return "Invalid query. When querying a collection by "
             "FieldPath.documentId() you must provide a plain document ID, but "
             "'foo/bar/baz' contains a '/' character.";
#else
      return "Invalid query. When querying a collection by document ID you "
             "must provide a plain document ID, but 'foo/bar/baz' contains a "
             "'/' character.";
#endif

    case ErrorCase::kQueryDocumentIdInteger:
#ifdef __ANDROID__
      return "Invalid query. When querying with FieldPath.documentId() you "
             "must provide a valid String or DocumentReference, but it was of "
             "type: java.lang.Long";
#else
      return "Invalid query. When querying by document ID you must provide a "
             "valid string or DocumentReference, but it was of type: "
             "FieldValue::Integer()";
#endif

    case ErrorCase::kQueryDocumentIdArrayContains:
#ifdef __ANDROID__
      return "Invalid query. You can't perform 'array_contains' queries on "
             "FieldPath.documentId().";
#else
      return "Invalid query. You can't perform arrayContains queries on "
             "document ID since document IDs are not arrays.";
#endif
  }

  FIRESTORE_UNREACHABLE();
}

}  // namespace

class ValidationTest : public FirestoreIntegrationTest {
 protected:
  /**
   * Performs a write using each write API and makes sure it fails with the
   * expected reason.
   */
  void ExpectWriteError(const MapFieldValue& data, const std::string& reason) {
    ExpectWriteError(data, reason, /*include_sets=*/true,
                     /*include_updates=*/true);
  }

  /**
   * Performs a write using each update API and makes sure it fails with the
   * expected reason.
   */
  void ExpectUpdateError(const MapFieldValue& data, const std::string& reason) {
    ExpectWriteError(data, reason, /*include_sets=*/false,
                     /*include_updates=*/true);
  }

  /**
   * Performs a write using each set API and makes sure it fails with the
   * expected reason.
   */
  void ExpectSetError(const MapFieldValue& data, const std::string& reason) {
    ExpectWriteError(data, reason, /*include_sets=*/true,
                     /*include_updates=*/false);
  }

  /**
   * Performs a write using each set and/or update API and makes sure it fails
   * with the expected reason.
   */
  void ExpectWriteError(const MapFieldValue& data,
                        const std::string& reason,
                        bool include_sets,
                        bool include_updates) {
    DocumentReference document = Document();

    if (include_sets) {
      EXPECT_ERROR(document.Set(data), reason);
      EXPECT_ERROR(TestFirestore()->batch().Set(document, data), reason);
    }

    if (include_updates) {
      EXPECT_ERROR(document.Update(data), reason);
      EXPECT_ERROR(TestFirestore()->batch().Update(document, data), reason);
    }

    Await(TestFirestore()->RunTransaction(
        [data, reason, include_sets, include_updates, document](
            Transaction& transaction, std::string& error_message) -> Error {
          if (include_sets) {
            EXPECT_ERROR(transaction.Set(document, data), reason);
          }
          if (include_updates) {
            EXPECT_ERROR(transaction.Update(document, data), reason);
          }
          return Error::kErrorOk;
        }));
  }

  /**
   * Tests a field path with all of our APIs that accept field paths and ensures
   * they fail with the specified reason.
   */
  void VerifyFieldPathThrows(const std::string& path,
                             const std::string& reason) {
    // Get an arbitrary snapshot we can use for testing.
    DocumentReference document = Document();
    WriteDocument(document, MapFieldValue{{"test", FieldValue::Integer(1)}});
    DocumentSnapshot snapshot = ReadDocument(document);

    // snapshot paths
    EXPECT_ERROR(snapshot.Get(path), reason);

    // Query filter / order fields
    CollectionReference collection = Collection();
    // WhereLessThan(), etc. omitted for brevity since the code path is
    // trivially shared.
    EXPECT_ERROR(collection.WhereEqualTo(path, FieldValue::Integer(1)), reason);
    EXPECT_ERROR(collection.OrderBy(path), reason);

    // update() paths.
    EXPECT_ERROR_EITHER(
        document.Update(MapFieldValue{{path, FieldValue::Integer(1)}}), reason,
        // TODO(b/171990785): Unify Android and C++ validation error messages.
        // The Android SDK uses a different error message in this case.
        "Use FieldPath.of() for field names containing '~*/[]'.");
  }
};

// PORT_NOTE: Does not apply to C++ as host parameter is passed by value.
TEST_F(ValidationTest, FirestoreSettingsNullHostFails) {}

TEST_F(ValidationTest, ChangingSettingsAfterUseFails) {
  DocumentReference reference = Document();
  // Force initialization of the underlying client
  WriteDocument(reference, MapFieldValue{{"key", FieldValue::String("value")}});
  Settings setting;
  setting.set_host("foo");
  EXPECT_ERROR(TestFirestore()->set_settings(setting),
               ErrorMessage(ErrorCase::kSettingsAfterUse));
}

TEST_F(ValidationTest, DisableSslWithoutSettingHostFails) {
  Settings setting;
  setting.set_ssl_enabled(false);
  EXPECT_ERROR(TestFirestore()->set_settings(setting),
               ErrorMessage(ErrorCase::kSettingsDisableSsl));
}

TEST_F(ValidationTest, FirestoreGetInstanceWithNullAppFails) {
  EXPECT_ERROR(
      Firestore::GetInstance(/*app=*/nullptr, /*init_result=*/nullptr),
      "firebase::App instance cannot be null. Use "
      "firebase::App::GetInstance() without arguments if you'd like to use "
      "the default instance.");
}

TEST_F(ValidationTest,
       FirestoreGetInstanceWithNonNullAppReturnsNonNullInstance) {
  InitResult result;
  EXPECT_NO_THROW(Firestore::GetInstance(app(), &result));
  EXPECT_EQ(kInitResultSuccess, result);
  EXPECT_NE(Firestore::GetInstance(app()), nullptr);
}

TEST_F(ValidationTest, CollectionPathsMustBeOddLength) {
  Firestore* db = TestFirestore();
  DocumentReference base_document = db->Document("foo/bar");
  std::vector<std::string> bad_absolute_paths = {"foo/bar", "foo/bar/baz/quu"};
  std::vector<std::string> bad_relative_paths = {"/", "baz/quu"};
  std::vector<std::string> expect_errors = {
      "Invalid collection reference. Collection references must have an odd "
      "number of segments, but foo/bar has 2",
      "Invalid collection reference. Collection references must have an odd "
      "number of segments, but foo/bar/baz/quu has 4",
  };
  for (int i = 0; i < expect_errors.size(); ++i) {
    EXPECT_ERROR(db->Collection(bad_absolute_paths[i]), expect_errors[i]);
    EXPECT_ERROR(base_document.Collection(bad_relative_paths[i]),
                 expect_errors[i]);
  }
}

TEST_F(ValidationTest, PathsMustNotHaveEmptySegments) {
  Firestore* db = TestFirestore();
  // NOTE: leading / trailing slashes are okay.
  db->Collection("/foo/");
  db->Collection("/foo");
  db->Collection("foo/");

  std::vector<std::string> bad_paths = {"foo//bar//baz", "//foo", "foo//"};
  CollectionReference collection = db->Collection("test-collection");
  DocumentReference document = collection.Document("test-document");
  for (const std::string& path : bad_paths) {
    std::string reason =
        "Invalid path (" + path + "). Paths must not contain // in them.";
    EXPECT_ERROR(db->Collection(path), reason);
    EXPECT_ERROR(db->Document(path), reason);
    EXPECT_ERROR(collection.Document(path), reason);
    EXPECT_ERROR(document.Collection(path), reason);
  }
}

TEST_F(ValidationTest, DocumentPathsMustBeEvenLength) {
  Firestore* db = TestFirestore();
  CollectionReference base_collection = db->Collection("foo");
  std::vector<std::string> bad_absolute_paths = {"foo", "foo/bar/baz"};
  std::vector<std::string> bad_relative_paths = {"/", "bar/baz"};
  std::vector<std::string> expect_errors = {
      "Invalid document reference. Document references must have an even "
      "number of segments, but foo has 1",
      "Invalid document reference. Document references must have an even "
      "number of segments, but foo/bar/baz has 3",
  };
  for (int i = 0; i < expect_errors.size(); ++i) {
    EXPECT_ERROR(db->Document(bad_absolute_paths[i]), expect_errors[i]);
    EXPECT_ERROR(base_collection.Document(bad_relative_paths[i]),
                 expect_errors[i]);
  }
}

// PORT_NOTE: Does not apply to C++ which is strong-typed.
TEST_F(ValidationTest, WritesMustBeMapsOrPOJOs) {}

TEST_F(ValidationTest, WritesMustNotContainDirectlyNestedLists) {
  ExpectWriteError(
      MapFieldValue{
          {"nested-array",
           FieldValue::Array({FieldValue::Integer(1),
                              FieldValue::Array({FieldValue::Integer(2)})})}},
      "Invalid data. Nested arrays are not supported");
}

TEST_F(ValidationTest, WritesMayContainIndirectlyNestedLists) {
  MapFieldValue data = {
      {"nested-array",
       FieldValue::Array(
           {FieldValue::Integer(1),
            FieldValue::Map({{"foo", FieldValue::Integer(2)}})})}};

  CollectionReference collection = Collection();
  DocumentReference document = collection.Document();
  DocumentReference another_document = collection.Document();

  Await(document.Set(data));
  Await(TestFirestore()->batch().Set(document, data).Commit());

  Await(document.Update(data));
  Await(TestFirestore()->batch().Update(document, data).Commit());

  Await(TestFirestore()->RunTransaction(
      [data, document, another_document](Transaction& transaction,
                                         std::string& error_message) -> Error {
        // Note another_document does not exist at this point so set that and
        // update document.
        transaction.Update(document, data);
        transaction.Set(another_document, data);
        return Error::kErrorOk;
      }));
}

TEST_F(ValidationTest, WritesMustNotContainReferencesToADifferentDatabase) {
  auto project_id = TestFirestore()->app()->options().project_id();
  DocumentReference ref =
      TestFirestoreWithProjectId(/*name=*/"db2", /*project_id=*/"different-db")
          ->Document("baz/quu");
  auto data = FieldValue::Reference(ref);

  ExpectWriteError(MapFieldValue{{"foo", data}},
                   "Invalid data. Document reference is for database "
                   "different-db/(default) but should be for database " +
                       std::string(project_id) +
                       "/(default) (found in field foo)");
}

TEST_F(ValidationTest, WritesMustNotContainReservedFieldNames) {
  SCOPED_TRACE("WritesMustNotContainReservedFieldNames");

  ExpectWriteError(MapFieldValue{{"__baz__", FieldValue::Integer(1)}},
                   "Invalid data. Document fields cannot begin and end with "
                   "\"__\" (found in field __baz__)");
  ExpectWriteError(
      MapFieldValue{
          {"foo", FieldValue::Map({{"__baz__", FieldValue::Integer(1)}})}},
      "Invalid data. Document fields cannot begin and end with \"__\" (found "
      "in "
      "field foo.__baz__)");
  ExpectWriteError(
      MapFieldValue{
          {"__baz__", FieldValue::Map({{"foo", FieldValue::Integer(1)}})}},
      "Invalid data. Document fields cannot begin and end with \"__\" (found "
      "in "
      "field __baz__)");

  ExpectUpdateError(MapFieldValue{{"__baz__", FieldValue::Integer(1)}},
                    "Invalid data. Document fields cannot begin and end with "
                    "\"__\" (found in field __baz__)");
  ExpectUpdateError(MapFieldValue{{"baz.__foo__", FieldValue::Integer(1)}},
                    "Invalid data. Document fields cannot begin and end with "
                    "\"__\" (found in field baz.__foo__)");
}

TEST_F(ValidationTest, SetsMustNotContainFieldValueDelete) {
  SCOPED_TRACE("SetsMustNotContainFieldValueDelete");

  ExpectSetError(MapFieldValue{{"foo", FieldValue::Delete()}},
                 ErrorMessage(ErrorCase::kFieldValueDeleteInSet));
}

TEST_F(ValidationTest, UpdatesMustNotContainNestedFieldValueDeletes) {
  SCOPED_TRACE("UpdatesMustNotContainNestedFieldValueDeletes");

  ExpectUpdateError(
      MapFieldValue{{"foo", FieldValue::Map({{"bar", FieldValue::Delete()}})}},
      ErrorMessage(ErrorCase::kFieldValueDeleteNested));
}

TEST_F(ValidationTest, BatchWritesRequireValidDocumentReferences) {
  std::string reason = "Invalid document reference provided.";

  MapFieldValue data{{"foo", FieldValue::Integer(1)}};
  DocumentReference bad_document;
  WriteBatch batch = TestFirestore()->batch();

  EXPECT_ERROR(batch.Set(bad_document, data), reason);
  EXPECT_ERROR(batch.Update(bad_document, data), reason);
  EXPECT_ERROR(batch.Delete(bad_document), reason);
}

TEST_F(ValidationTest, BatchWritesRequireCorrectDocumentReferences) {
  DocumentReference bad_document =
      TestFirestore("another")->Document("foo/bar");

  WriteBatch batch = TestFirestore()->batch();
  EXPECT_ERROR(
      batch.Set(bad_document, MapFieldValue{{"foo", FieldValue::Integer(1)}}),
      "Provided document reference is from a different Cloud Firestore "
      "instance.");
}

TEST_F(ValidationTest, TransactionsRequireValidDocumentReferences) {
  std::string reason = "Invalid document reference provided.";

  MapFieldValue data{{"foo", FieldValue::Integer(1)}};
  DocumentReference bad_ref;

  auto future =
      TestFirestore()->RunTransaction([&](Transaction& txn, std::string&) {
        EXPECT_ERROR(
            txn.Get(bad_ref, /*error_code=*/nullptr, /*error_message=*/nullptr),
            reason);
        EXPECT_ERROR(txn.Set(bad_ref, data), reason);
        EXPECT_ERROR(txn.Set(bad_ref, data, SetOptions::Merge()), reason);
        EXPECT_ERROR(txn.Update(bad_ref, data), reason);
        EXPECT_ERROR(txn.Delete(bad_ref), reason);

        return Error::kErrorOk;
      });

  EXPECT_THAT(future, FutureSucceeds());
}

TEST_F(ValidationTest, TransactionsRequireCorrectDocumentReferences) {
  auto* db1 = TestFirestore();
  auto* db2 = TestFirestore("db2");
  EXPECT_NE(db1, db2);

  std::string reason =
      "Provided document reference is from a different Cloud Firestore "
      "instance.";
  MapFieldValue data{{"foo", FieldValue::Integer(1)}};
  DocumentReference bad_ref = db2->Document("foo/bar");

  // TODO(b/194338435): fix the discrepancy between Android and other platforms.
#if defined(__ANDROID__)
  auto future = db1->RunTransaction([&](Transaction& txn, std::string&) {
    txn.Get(bad_ref, /*error_code=*/nullptr, /*error_message=*/nullptr);
    txn.Set(bad_ref, data);
    txn.Set(bad_ref, data, SetOptions::Merge());
    txn.Update(bad_ref, data);
    txn.Delete(bad_ref);

    return Error::kErrorOk;
  });

  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnknown);
  EXPECT_STREQ(future.error_message(), reason.c_str());

#else
  auto future = db1->RunTransaction([&](Transaction& txn, std::string&) {
    EXPECT_ERROR(
        txn.Get(bad_ref, /*error_code=*/nullptr, /*error_message=*/nullptr),
        reason);
    EXPECT_ERROR(txn.Set(bad_ref, data), reason);
    EXPECT_ERROR(txn.Set(bad_ref, data, SetOptions::Merge()), reason);
    EXPECT_ERROR(txn.Update(bad_ref, data), reason);
    EXPECT_ERROR(txn.Delete(bad_ref), reason);

    return Error::kErrorOk;
  });

  EXPECT_THAT(future, FutureSucceeds());

#endif  // defined(__ANDROID__)
}

TEST_F(ValidationTest, FieldPathsMustNotHaveEmptySegments) {
  SCOPED_TRACE("FieldPathsMustNotHaveEmptySegments");

  std::vector<std::string> bad_field_paths = {"", "foo..baz", ".foo", "foo."};

  for (const auto field_path : bad_field_paths) {
    std::string reason = "Invalid field path (" + field_path +
                         "). Paths must not be empty, begin with '.', end with "
                         "'.', or contain '..'";
    VerifyFieldPathThrows(field_path, reason);
  }
}

TEST_F(ValidationTest, FieldPathsMustNotHaveInvalidSegments) {
  SCOPED_TRACE("FieldPathsMustNotHaveInvalidSegments");

  std::vector<std::string> bad_field_paths = {
      "foo~bar", "foo*bar", "foo/bar", "foo[1", "foo]1", "foo[1]",
  };

  for (const auto field_path : bad_field_paths) {
    std::string reason = "Invalid field path (" + field_path +
                         "). Paths must not contain '~', '*', '/', '[', or ']'";
    VerifyFieldPathThrows(field_path, reason);
  }
}

TEST_F(ValidationTest, FieldNamesMustNotBeEmpty) {
  DocumentSnapshot snapshot = ReadDocument(Document());
  // TODO(b/136012313): We do not enforce any logic for invalid C++ object. In
  // particular the creation of invalid object should be valid (for using
  // standard container). We have not defined the behavior to call API with
  // invalid object yet.
  // EXPECT_ERROR(snapshot.Get(FieldPath{}), "Invalid field path. Provided path
  // must not be empty.");

  EXPECT_ERROR(snapshot.Get(FieldPath{""}),
               "Invalid field name at index 0. Field names must not be empty.");
  EXPECT_ERROR(snapshot.Get(FieldPath{"foo", ""}),
               "Invalid field name at index 1. Field names must not be empty.");
}

TEST_F(ValidationTest, ArrayTransformsFailInQueries) {
  CollectionReference collection = Collection();
  EXPECT_ERROR(
      collection.WhereEqualTo(
          "test", FieldValue::Map({{"test", FieldValue::ArrayUnion(
                                                {FieldValue::Integer(1)})}})),
      ErrorMessage(ErrorCase::kArrayUnionInQuery));

  EXPECT_ERROR(
      collection.WhereEqualTo(
          "test", FieldValue::Map({{"test", FieldValue::ArrayRemove(
                                                {FieldValue::Integer(1)})}})),
      ErrorMessage(ErrorCase::kArrayRemoveInQuery));
}

// PORT_NOTE: Does not apply to C++ which is strong-typed.
TEST_F(ValidationTest, ArrayTransformsRejectInvalidElements) {}

TEST_F(ValidationTest, ArrayTransformsRejectArrays) {
  DocumentReference document = Document();
  // This would result in a directly nested array which is not supported.
  EXPECT_ERROR(
      document.Set(MapFieldValue{
          {"x", FieldValue::ArrayUnion(
                    {FieldValue::Integer(1),
                     FieldValue::Array({FieldValue::String("nested")})})}}),
      "Invalid data. Nested arrays are not supported");
  EXPECT_ERROR(
      document.Set(MapFieldValue{
          {"x", FieldValue::ArrayRemove(
                    {FieldValue::Integer(1),
                     FieldValue::Array({FieldValue::String("nested")})})}}),
      "Invalid data. Nested arrays are not supported");
}

TEST_F(ValidationTest, QueriesWithNonPositiveLimitFail) {
  CollectionReference collection = Collection();
  EXPECT_ERROR(
      collection.Limit(0),
      "Invalid Query. Query limit (0) is invalid. Limit must be positive.");
  EXPECT_ERROR(
      collection.Limit(-1),
      "Invalid Query. Query limit (-1) is invalid. Limit must be positive.");
}

TEST_F(ValidationTest, QueriesCannotBeCreatedFromDocumentsMissingSortValues) {
  CollectionReference collection =
      Collection(std::map<std::string, MapFieldValue>{
          {"f", MapFieldValue{{"k", FieldValue::String("f")},
                              {"nosort", FieldValue::Double(1.0)}}}});

  Query query = collection.OrderBy("sort");
  DocumentSnapshot snapshot = ReadDocument(collection.Document("f"));

  EXPECT_THAT(snapshot.GetData(), testing::ContainerEq(MapFieldValue{
                                      {"k", FieldValue::String("f")},
                                      {"nosort", FieldValue::Double(1.0)}}));

  std::string reason = ErrorMessage(ErrorCase::kQueryMissingOrderBy);
  EXPECT_ERROR(query.StartAt(snapshot), reason);
  EXPECT_ERROR(query.StartAfter(snapshot), reason);
  EXPECT_ERROR(query.EndBefore(snapshot), reason);
  EXPECT_ERROR(query.EndAt(snapshot), reason);
}

TEST_F(ValidationTest, QueriesCannotBeSortedByAnUncommittedServerTimestamp) {
  CollectionReference collection = Collection();
  EventAccumulator<QuerySnapshot> accumulator;
  accumulator.listener()->AttachTo(&collection);

  Await(TestFirestore()->DisableNetwork());

  Future<void> future = collection.Document("doc").Set(
      {{"timestamp", FieldValue::ServerTimestamp()}});

  QuerySnapshot snapshot = accumulator.Await();
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());

  snapshot = accumulator.Await();
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());

  EXPECT_ERROR(collection.OrderBy(FieldPath({"timestamp"}))
                   .EndAt(snapshot.documents().at(0))
                   .AddSnapshotListener(
                       [](const QuerySnapshot&, Error, const std::string&) {}),
               "Invalid query. You are trying to start or end a query using a "
               "document for which the field 'timestamp' is an uncommitted "
               "server timestamp. (Since the value of this field is unknown, "
               "you cannot start/end a query with it.)");

  Await(TestFirestore()->EnableNetwork());
  Await(future);

  snapshot = accumulator.AwaitRemoteEvent();
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_NO_THROW(collection.OrderBy(FieldPath({"timestamp"}))
                      .EndAt(snapshot.documents().at(0))
                      .AddSnapshotListener([](const QuerySnapshot&, Error,
                                              const std::string&) {}));
}

TEST_F(ValidationTest, QueriesMustNotHaveMoreComponentsThanOrderBy) {
  CollectionReference collection = Collection();
  Query query = collection.OrderBy("foo");

  std::string reason = ErrorMessage(ErrorCase::kQueryOrderByTooManyArguments);
  EXPECT_ERROR(query.StartAt({FieldValue::Integer(1), FieldValue::Integer(2)}),
               reason);
  EXPECT_ERROR(query.OrderBy("bar").StartAt({FieldValue::Integer(1),
                                             FieldValue::Integer(2),
                                             FieldValue::Integer(3)}),
               reason);
}

TEST_F(ValidationTest, QueryOrderByKeyBoundsMustBeStringsWithoutSlashes) {
  CollectionReference collection = Collection();
  Query query = collection.OrderBy(FieldPath::DocumentId());
  EXPECT_ERROR(query.StartAt({FieldValue::Integer(1)}),
               ErrorMessage(ErrorCase::kQueryInvalidBoundInteger));
  EXPECT_ERROR(query.StartAt({FieldValue::String("foo/bar")}),
               ErrorMessage(ErrorCase::kQueryInvalidBoundWithSlash));
}

TEST_F(ValidationTest, QueriesWithDifferentInequalityFieldsFail) {
  EXPECT_ERROR(Collection()
                   .WhereGreaterThan("x", FieldValue::Integer(32))
                   .WhereLessThan("y", FieldValue::String("cat")),
               ErrorMessage(ErrorCase::kQueryDifferentInequalityFields));
}

TEST_F(ValidationTest, QueriesWithInequalityDifferentThanFirstOrderByFail) {
  CollectionReference collection = Collection();
  std::string reason =
      ErrorMessage(ErrorCase::kQueryInequalityOrderByDifferentFields);
  EXPECT_ERROR(
      collection.WhereGreaterThan("x", FieldValue::Integer(32)).OrderBy("y"),
      reason);
  EXPECT_ERROR(
      collection.OrderBy("y").WhereGreaterThan("x", FieldValue::Integer(32)),
      reason);
  EXPECT_ERROR(collection.WhereGreaterThan("x", FieldValue::Integer(32))
                   .OrderBy("y")
                   .OrderBy("x"),
               reason);
  EXPECT_ERROR(collection.OrderBy("y").OrderBy("x").WhereGreaterThan(
                   "x", FieldValue::Integer(32)),
               reason);
}

TEST_F(ValidationTest, QueriesMustNotSpecifyStartingOrEndingPointAfterOrderBy) {
  CollectionReference collection = Collection();
  Query query = collection.OrderBy("foo");

  EXPECT_ERROR(query.StartAt({FieldValue::Integer(1)}).OrderBy("bar"),
               ErrorMessage(ErrorCase::kQueryStartBoundWithoutOrderBy));

  EXPECT_ERROR(query.StartAfter({FieldValue::Integer(1)}).OrderBy("bar"),
               ErrorMessage(ErrorCase::kQueryStartBoundWithoutOrderBy));

  EXPECT_ERROR(query.EndAt({FieldValue::Integer(1)}).OrderBy("bar"),
               ErrorMessage(ErrorCase::kQueryEndBoundWithoutOrderBy));

  EXPECT_ERROR(query.EndBefore({FieldValue::Integer(1)}).OrderBy("bar"),
               ErrorMessage(ErrorCase::kQueryEndBoundWithoutOrderBy));
}

TEST_F(ValidationTest,
       QueriesFilteredByDocumentIDMustUseStringsOrDocumentReferences) {
  CollectionReference collection = Collection();

  EXPECT_ERROR(collection.WhereGreaterThanOrEqualTo(FieldPath::DocumentId(),
                                                    FieldValue::String("")),
               ErrorMessage(ErrorCase::kQueryDocumentIdEmpty));

  EXPECT_ERROR(collection.WhereGreaterThanOrEqualTo(
                   FieldPath::DocumentId(), FieldValue::String("foo/bar/baz")),
               ErrorMessage(ErrorCase::kQueryDocumentIdSlash));

  EXPECT_ERROR(collection.WhereGreaterThanOrEqualTo(FieldPath::DocumentId(),
                                                    FieldValue::Integer(1)),
               ErrorMessage(ErrorCase::kQueryDocumentIdInteger));

  EXPECT_ERROR(collection.WhereArrayContains(FieldPath::DocumentId(),
                                             FieldValue::Integer(1)),
               ErrorMessage(ErrorCase::kQueryDocumentIdArrayContains));
}

#endif  // FIRESTORE_HAVE_EXCEPTIONS

}  // namespace firestore
}  // namespace firebase
