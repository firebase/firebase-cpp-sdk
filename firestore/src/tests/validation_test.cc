#include <cmath>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/src/log.h"
#include "auth/src/include/firebase/auth.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"
#include "firebase/firestore/firestore_errors.h"
#include "Firestore/core/src/util/firestore_exceptions.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRValidationTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/ValidationTest.java
//
// PORT_NOTE: C++ API Guidelines (http://g3doc/firebase/g3doc/cpp-api-style.md)
// discourage the use of exceptions in the Firebase Games's SDK. So in release,
// we do not throw exception while only dump exception info to logs. However, in
// order to test this behavior, we enable exception here and check exceptions.

namespace firebase {
namespace firestore {

// This eventually works for iOS as well and becomes the cross-platform test for
// C++ client SDK. For now, only enabled for Android platform.

#if FIRESTORE_HAVE_EXCEPTIONS

using firebase::auth::Auth;
using testing::AnyOf;
using testing::Property;
using testing::StrEq;
using testing::Throws;

#define EXPECT_ERROR(stmt, message)                           \
  EXPECT_THAT([&] { stmt; }, Throws<std::exception>(Property( \
                                 &std::exception::what, StrEq(message))));

/**
 * Creates a Matcher that matches any of a number of alternative error messages.
 * This is useful temporarily because the different platforms produce different
 * error messages from the same validation criteria.
 *
 * Every usage of this method is an indication of a disagreement between these
 * platforms and ultimately must be eliminated.
 *
 * TODO(b/171990785): Unify Android and C++ validation error messages.
 * Remove this method once that's done.
 */
template <typename... Messages>
auto AnyMessageEq(Messages&&... messages)
    -> decltype(AnyOf(StrEq(messages)...)) {
  return AnyOf(StrEq(messages)...);
}

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
  void ExpectWriteError(const MapFieldValue& data, const std::string& reason,
                        bool include_sets, bool include_updates) {
    DocumentReference document = Document();

    if (include_sets) {
      EXPECT_ERROR(document.Set(data), reason);
      EXPECT_ERROR(TestFirestore()->batch().Set(document, data), reason);
    }

    if (include_updates) {
      EXPECT_ERROR(document.Update(data), reason);
      EXPECT_ERROR(TestFirestore()->batch().Update(document, data), reason);
    }

#if defined(FIREBASE_USE_STD_FUNCTION)
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
#endif  // defined(FIREBASE_USE_STD_FUNCTION)
  }

  /**
   * Tests a field path with all of our APIs that accept field paths and ensures
   * they fail with the specified reason.
   */
  // TODO(varconst): this function is pretty much commented out.
  void VerifyFieldPathThrows(const std::string& path,
                             const std::string& reason) {
    // Get an arbitrary snapshot we can use for testing.
    DocumentReference document = Document();
    WriteDocument(document, MapFieldValue{{"test", FieldValue::Integer(1)}});
    DocumentSnapshot snapshot = ReadDocument(document);

    // snapshot paths
    try {
      snapshot.Get(path);
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(reason, exception.what());
    }

    // Query filter / order fields
    CollectionReference collection = Collection();
    // WhereLessThan(), etc. omitted for brevity since the code path is
    // trivially shared.
    try {
      collection.WhereEqualTo(path, FieldValue::Integer(1));
      FAIL() << "should throw exception" << path;
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(reason, exception.what());
    }
    try {
      collection.OrderBy(path);
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(reason, exception.what());
    }

    // update() paths.
    try {
      document.Update(MapFieldValue{{path, FieldValue::Integer(1)}});
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_THAT(
          exception.what(),
          AnyMessageEq(
              reason,
              "Use FieldPath.of() for field names containing '~*/[]'."));
    }
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
  try {
    TestFirestore()->set_settings(setting);
    FAIL() << "should throw exception";
  } catch (const std::logic_error& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Firestore instance has already been started and its settings can "
            "no longer be changed. You can only set settings before calling "
            "any other methods on a Firestore instance.",
            "FirebaseFirestore has already been started and its settings can "
            "no longer be changed. You can only call setFirestoreSettings() "
            "before calling any other methods on a FirebaseFirestore object."));
  }
}

TEST_F(ValidationTest, DisableSslWithoutSettingHostFails) {
  Settings setting;
  setting.set_ssl_enabled(false);
  try {
    TestFirestore()->set_settings(setting);
    FAIL() << "should throw exception";
  } catch (const std::logic_error& exception) {
    EXPECT_STREQ(
        "You can't set the 'sslEnabled' setting unless you also set a "
        "non-default 'host'.",
        exception.what());
  }
}

// PORT_NOTE: Does not apply to C++ as host parameter is passed by value.
TEST_F(ValidationTest, FirestoreGetInstanceWithNullAppFails) {}

TEST_F(ValidationTest,
       FirestoreGetInstanceWithNonNullAppReturnsNonNullInstance) {
  try {
    // The app instance is deleted by FirestoreIntegrationTest.
    auto* app = this->app();

    InitResult init_result;
    auto auth = UniquePtr<Auth>(Auth::GetAuth(app, &init_result));
#if defined(__ANDROID__)
    if (init_result != kInitResultSuccess) {
      // On Android, it's possible for the Auth library built at head to be too
      // new for the version of Play Services available in the Android emulator.
      // In this case, Auth will fail to initialize. Meanwhile, there's no
      // simple way to detect if the Android app is running in an emulator
      // running on Forge. Consequently, just punt if Auth fails to initialize.
      LogWarning(
          "Skipped FirestoreGetInstanceWithNonNullAppReturnsNonNullInstance "
          "test: Auth missing or failed to initialize");
      return;
    }
#else
    ASSERT_EQ(init_result, kInitResultSuccess);
#endif

    auto db = UniquePtr<Firestore>(Firestore::GetInstance(app, &init_result));
    EXPECT_EQ(kInitResultSuccess, init_result);
  } catch (const std::exception& exception) {
    FAIL() << "shouldn't throw exception";
  }
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
    try {
      db->Collection(bad_absolute_paths[i]);
      FAIL() << "should throw exception";
    } catch (const std::exception& exception) {
      EXPECT_EQ(expect_errors[i], exception.what());
    }
    try {
      base_document.Collection(bad_relative_paths[i]);
      FAIL() << "should throw exception";
    } catch (const std::exception& exception) {
      EXPECT_EQ(expect_errors[i], exception.what());
    }
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
    try {
      db->Collection(path);
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(reason, exception.what());
    }
    try {
      db->Document(path);
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(reason, exception.what());
    }
    try {
      collection.Document(path);
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(reason, exception.what());
    }
    try {
      document.Collection(path);
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(reason, exception.what());
    }
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
    try {
      db->Document(bad_absolute_paths[i]);
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(expect_errors[i], exception.what());
    }
    try {
      base_collection.Document(bad_relative_paths[i]);
      FAIL() << "should throw exception";
    } catch (const std::invalid_argument& exception) {
      EXPECT_EQ(expect_errors[i], exception.what());
    }
  }
}

// PORT_NOTE: Does not apply to C++ which is strong-typed.
TEST_F(ValidationTest, WritesMustBeMapsOrPOJOs) {}

TEST_F(ValidationTest, WritesMustNotContainDirectlyNestedLists) {
  SCOPED_TRACE("WritesMustNotContainDirectlyNestedLists");

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

#if defined(FIREBASE_USE_STD_FUNCTION)
  Await(TestFirestore()->RunTransaction(
      [data, document, another_document](Transaction& transaction,
                                         std::string& error_message) -> Error {
        // Note another_document does not exist at this point so set that and
        // update document.
        transaction.Update(document, data);
        transaction.Set(another_document, data);
        return Error::kErrorOk;
      }));
#endif  // defined(FIREBASE_USE_STD_FUNCTION)
}

// TODO(zxu): There is no way to create Firestore with different project id yet.
TEST_F(ValidationTest, WritesMustNotContainReferencesToADifferentDatabase) {}

TEST_F(ValidationTest, WritesMustNotContainReservedFieldNames) {
  SCOPED_TRACE("WritesMustNotContainReservedFieldNames");

  ExpectWriteError(MapFieldValue{{"__baz__", FieldValue::Integer(1)}},
                   "Invalid data. Document fields cannot begin and end with "
                   "\"__\" (found in field __baz__)");
  ExpectWriteError(
      MapFieldValue{
          {"foo", FieldValue::Map({{"__baz__", FieldValue::Integer(1)}})}},
      "Invalid data. Document fields cannot begin and end with \"__\" (found "
      "in field foo.__baz__)");
  ExpectWriteError(
      MapFieldValue{
          {"__baz__", FieldValue::Map({{"foo", FieldValue::Integer(1)}})}},
      "Invalid data. Document fields cannot begin and end with \"__\" (found "
      "in field __baz__)");

  ExpectUpdateError(MapFieldValue{{"__baz__", FieldValue::Integer(1)}},
                    "Invalid data. Document fields cannot begin and end with "
                    "\"__\" (found in field __baz__)");
  ExpectUpdateError(MapFieldValue{{"baz.__foo__", FieldValue::Integer(1)}},
                    "Invalid data. Document fields cannot begin and end with "
                    "\"__\" (found in field baz.__foo__)");
}

TEST_F(ValidationTest, SetsMustNotContainFieldValueDelete) {
  SCOPED_TRACE("SetsMustNotContainFieldValueDelete");

  // TODO(b/171990785): Unify Android and C++ validation error messages.
#if defined(__ANDROID__)
  std::string message =
      "Invalid data. FieldValue.delete() can only be used with update() and "
      "set() with SetOptions.merge() (found in field foo)";
#else
  std::string message =
      "Invalid data. FieldValue::Delete() can only be used with Update() and "
      "Set() with merge == true (found in field foo)";
#endif  // defined(__ANDROID__)

  ExpectSetError(MapFieldValue{{"foo", FieldValue::Delete()}}, message);
}

TEST_F(ValidationTest, UpdatesMustNotContainNestedFieldValueDeletes) {
  SCOPED_TRACE("UpdatesMustNotContainNestedFieldValueDeletes");

  // TODO(b/171990785): Unify Android and C++ validation error messages.
#if defined(__ANDROID__)
  std::string message =
      "Invalid data. FieldValue.delete() can only appear at the top level of "
      "your update data (found in field foo.bar)";
#else
  std::string message =
      "Invalid data. FieldValue::Delete() can only appear at the top level of "
      "your update data (found in field foo.bar)";
#endif  // defined(__ANDROID__)

  ExpectUpdateError(
      MapFieldValue{{"foo", FieldValue::Map({{"bar", FieldValue::Delete()}})}},
      message);
}

TEST_F(ValidationTest, BatchWritesRequireCorrectDocumentReferences) {
  DocumentReference bad_document =
      TestFirestore("another")->Document("foo/bar");

  WriteBatch batch = TestFirestore()->batch();
  try {
    batch.Set(bad_document, MapFieldValue{{"foo", FieldValue::Integer(1)}});
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_STREQ(
        "Provided document reference is from a different Cloud Firestore "
        "instance.",
        exception.what());
  }
}

TEST_F(ValidationTest, TransactionsRequireCorrectDocumentReferences) {}

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
  // PORT_NOTE: We do not enforce any logic for invalid C++ object. In
  // particular the creation of invalid object should be valid (for using
  // standard container). We have not defined the behavior to call API with
  // invalid object yet.
  // try {
  //   snapshot.Get(FieldPath{});
  //   FAIL() << "should throw exception";
  // } catch (const std::exception& exception) {
  //  EXPECT_STREQ("Invalid field path. Provided path must not be empty.",
  //               exception.what());
  // }
  try {
    snapshot.Get(FieldPath{""});
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid field name at index 0. Field names must not be empty.",
            "Invalid field name at argument 1. Field names must not be null or "
            "empty."));
  }
  try {
    snapshot.Get(FieldPath{"foo", ""});
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid field name at index 1. Field names must not be empty.",
            "Invalid field name at argument 2. Field names must not be null or "
            "empty."));
  }
}

TEST_F(ValidationTest, ArrayTransformsFailInQueries) {
  CollectionReference collection = Collection();
  try {
    collection.WhereEqualTo(
        "test",
        FieldValue::Map(
            {{"test", FieldValue::ArrayUnion({FieldValue::Integer(1)})}}));
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid data. FieldValue::ArrayUnion() can only be used with "
            "Update() and Set() (found in field test)",
            "Invalid data. FieldValue.arrayUnion() can only be used with set() "
            "and update() (found in field test)"));
  }

  try {
    collection.WhereEqualTo(
        "test",
        FieldValue::Map(
            {{"test", FieldValue::ArrayRemove({FieldValue::Integer(1)})}}));
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid data. FieldValue::ArrayRemove() can only be used with "
            "Update() and Set() (found in field test)",
            "Invalid data. FieldValue.arrayRemove() can only be used with "
            "set() and update() (found in field test)"));
  }
}

// PORT_NOTE: Does not apply to C++ which is strong-typed.
TEST_F(ValidationTest, ArrayTransformsRejectInvalidElements) {}

TEST_F(ValidationTest, ArrayTransformsRejectArrays) {
  DocumentReference document = Document();
  // This would result in a directly nested array which is not supported.
  try {
    document.Set(MapFieldValue{
        {"x", FieldValue::ArrayUnion(
                  {FieldValue::Integer(1),
                   FieldValue::Array({FieldValue::String("nested")})})}});
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_STREQ("Invalid data. Nested arrays are not supported",
                 exception.what());
  }
  try {
    document.Set(MapFieldValue{
        {"x", FieldValue::ArrayRemove(
                  {FieldValue::Integer(1),
                   FieldValue::Array({FieldValue::String("nested")})})}});
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_STREQ("Invalid data. Nested arrays are not supported",
                 exception.what());
  }
}

TEST_F(ValidationTest, QueriesWithNonPositiveLimitFail) {
  CollectionReference collection = Collection();
  try {
    collection.Limit(0);
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_STREQ(
        "Invalid Query. Query limit (0) is invalid. Limit must be positive.",
        exception.what());
  }
  try {
    collection.Limit(-1);
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_STREQ(
        "Invalid Query. Query limit (-1) is invalid. Limit must be positive.",
        exception.what());
  }
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

  auto matches = AnyMessageEq(
      "Invalid query. You are trying to start or end a query using a document "
      "for which the field 'sort' (used as the order by) does not exist.",
      "Invalid query. You are trying to start or end a query using a document "
      "for which the field 'sort' (used as the orderBy) does not exist.");

  try {
    query.StartAt(snapshot);
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(exception.what(), matches);
  }
  try {
    query.StartAfter(snapshot);
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(exception.what(), matches);
  }
  try {
    query.EndBefore(snapshot);
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(exception.what(), matches);
  }
  try {
    query.EndAt(snapshot);
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(exception.what(), matches);
  }
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

  EXPECT_THROW(collection.OrderBy(FieldPath({"timestamp"}))
                   .EndAt(snapshot.documents().at(0))
                   .AddSnapshotListener(
                       [](const QuerySnapshot&, Error, const std::string&) {}),
               std::exception);

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

  auto matches = AnyMessageEq(
      "Invalid query. You are trying to start or end a query using more values "
      "than were specified in the order by.",
      "Too many arguments provided to startAt(). The number of arguments must "
      "be less than or equal to the number of orderBy() clauses.");
  try {
    query.StartAt({FieldValue::Integer(1), FieldValue::Integer(2)});
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(exception.what(), matches);
  }
  try {
    query.OrderBy("bar").StartAt({FieldValue::Integer(1),
                                  FieldValue::Integer(2),
                                  FieldValue::Integer(3)});
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(exception.what(), matches);
  }
}

TEST_F(ValidationTest, QueryOrderByKeyBoundsMustBeStringsWithoutSlashes) {
  CollectionReference collection = Collection();
  Query query = collection.OrderBy(FieldPath::DocumentId());
  try {
    query.StartAt({FieldValue::Integer(1)});
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. Expected a string for the document ID.",
            "Invalid query. Expected a string for document ID in startAt(), "
            "but got 1."));
  }
  try {
    query.StartAt({FieldValue::String("foo/bar")});
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. When querying a collection and ordering by "
            "document ID, you must pass a plain document ID, but 'foo/bar' "
            "contains a slash.",
            "Invalid query. When querying a collection and ordering by "
            "FieldPath.documentId(), the value passed to startAt() must be a "
            "plain document ID, but 'foo/bar' contains a slash."));
  }
}

TEST_F(ValidationTest, QueriesWithDifferentInequalityFieldsFail) {
  try {
    Collection()
        .WhereGreaterThan("x", FieldValue::Integer(32))
        .WhereLessThan("y", FieldValue::String("cat"));
    FAIL() << "should throw exception";
  } catch (const std::invalid_argument& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid Query. All where filters with an inequality (notEqual, "
            "lessThan, lessThanOrEqual, greaterThan, or greaterThanOrEqual) "
            "must be on the same field. But you have inequality filters on 'x' "
            "and 'y'",
            "All where filters with an inequality (notEqualTo, notIn, "
            "lessThan, lessThanOrEqualTo, greaterThan, or "
            "greaterThanOrEqualTo) must be on the same field. But you have "
            "filters on 'x' and 'y'"));
  }
}

TEST_F(ValidationTest, QueriesWithInequalityDifferentThanFirstOrderByFail) {
  // TODO(b/171990785): Unify Android and C++ validation error messages.
#if defined(__ANDROID__)
  const char* reason =
      "Invalid query. You have an inequality where filter (whereLessThan(), "
      "whereGreaterThan(), etc.) on field 'x' and so you must also have 'x' as "
      "your first orderBy() field, but your first orderBy() is currently on "
      "field 'y' instead.";
#else
  const char* reason =
      "Invalid query. You have a where filter with an inequality (notEqual, "
      "lessThan, lessThanOrEqual, greaterThan, or greaterThanOrEqual) on field "
      "'x' and so you must also use 'x' as your first queryOrderedBy field, "
      "but your first queryOrderedBy is currently on field 'y' instead.";
#endif  // defined(__ANDROID__)

  CollectionReference collection = Collection();
  try {
    collection.WhereGreaterThan("x", FieldValue::Integer(32)).OrderBy("y");
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_STREQ(exception.what(), reason);
  }
  try {
    collection.OrderBy("y").WhereGreaterThan("x", FieldValue::Integer(32));
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_STREQ(exception.what(), reason);
  }
  try {
    collection.WhereGreaterThan("x", FieldValue::Integer(32))
        .OrderBy("y")
        .OrderBy("x");
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_STREQ(exception.what(), reason);
  }
  try {
    collection.OrderBy("y").OrderBy("x").WhereGreaterThan(
        "x", FieldValue::Integer(32));
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_STREQ(exception.what(), reason);
  }
}

TEST_F(ValidationTest, QueriesWithMultipleArrayContainsFiltersFail) {
  try {
    Collection()
        .WhereArrayContains("foo", FieldValue::Integer(1))
        .WhereArrayContains("foo", FieldValue::Integer(2));
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid Query. You cannot use more than one 'arrayContains' "
            "filter.",
            "Invalid Query. You cannot use more than one 'array_contains' "
            "filter."));
  }
}

TEST_F(ValidationTest, QueriesMustNotSpecifyStartingOrEndingPointAfterOrderBy) {
  CollectionReference collection = Collection();
  Query query = collection.OrderBy("foo");
  try {
    query.StartAt({FieldValue::Integer(1)}).OrderBy("bar");
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. You must not specify a starting point before "
            "specifying the order by.",
            "Invalid query. You must not call Query.startAt() or "
            "Query.startAfter() before calling Query.orderBy()."));
  }
  try {
    query.StartAfter({FieldValue::Integer(1)}).OrderBy("bar");
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. You must not specify a starting point before "
            "specifying the order by.",
            "Invalid query. You must not call Query.startAt() or "
            "Query.startAfter() before calling Query.orderBy()."));
  }
  try {
    query.EndAt({FieldValue::Integer(1)}).OrderBy("bar");
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. You must not specify an ending point before "
            "specifying the order by.",
            "Invalid query. You must not call Query.endAt() or "
            "Query.endBefore() before calling Query.orderBy()."));
  }
  try {
    query.EndBefore({FieldValue::Integer(1)}).OrderBy("bar");
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. You must not specify an ending point before "
            "specifying the order by.",
            "Invalid query. You must not call Query.endAt() or "
            "Query.endBefore() before calling Query.orderBy()."));
  }
}

TEST_F(ValidationTest,
       QueriesFilteredByDocumentIDMustUseStringsOrDocumentReferences) {
  CollectionReference collection = Collection();
  try {
    collection.WhereGreaterThanOrEqualTo(FieldPath::DocumentId(),
                                         FieldValue::String(""));
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. When querying by document ID you must provide a "
            "valid document ID, but it was an empty string.",
            "Invalid query. When querying with FieldPath.documentId() you must "
            "provide a valid document ID, but it was an empty string."));
  }

  try {
    collection.WhereGreaterThanOrEqualTo(FieldPath::DocumentId(),
                                         FieldValue::String("foo/bar/baz"));
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. When querying a collection by document ID you must "
            "provide a plain document ID, but 'foo/bar/baz' contains a '/' "
            "character.",
            "Invalid query. When querying a collection by "
            "FieldPath.documentId() you must provide a plain document ID, but "
            "'foo/bar/baz' contains a '/' character."));
  }

  try {
    collection.WhereGreaterThanOrEqualTo(FieldPath::DocumentId(),
                                         FieldValue::Integer(1));
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. When querying by document ID you must provide a "
            "valid string or DocumentReference, but it was of type: "
            "FieldValue::Integer()",
            "Invalid query. When querying with FieldPath.documentId() you must "
            "provide a valid String or DocumentReference, but it was of type: "
            "java.lang.Long"));
  }

  try {
    collection.WhereArrayContains(FieldPath::DocumentId(),
                                  FieldValue::Integer(1));
    FAIL() << "should throw exception";
  } catch (const std::exception& exception) {
    EXPECT_THAT(
        exception.what(),
        AnyMessageEq(
            "Invalid query. You can't perform arrayContains queries on "
            "document ID since document IDs are not arrays.",
            "Invalid query. You can't perform 'array_contains' queries on "
            "FieldPath.documentId()."));
  }
}

#endif  // FIRESTORE_HAVE_EXCEPTIONS

}  // namespace firestore
}  // namespace firebase
