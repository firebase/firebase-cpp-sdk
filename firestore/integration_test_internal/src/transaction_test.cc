#include <utility>
#include <vector>

#if !defined(FIRESTORE_STUB_BUILD)
#include "app/src/mutex.h"
#include "app/src/semaphore.h"
#include "app/src/time.h"
#endif

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_join.h"
#include "firebase/firestore/firestore_errors.h"
#if defined(__ANDROID__)
#include "firestore/src/android/transaction_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/transaction_stub.h"

#endif  // defined(__ANDROID__)

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FSTTransactionTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/TransactionTest.java
//
// Some test cases are moved to transaction_extra_test.cc. If run together, the
// test will run too long and timeout.

namespace firebase {
namespace firestore {

// These tests don't work with the stubs.
#if !defined(FIRESTORE_STUB_BUILD)

using ::testing::HasSubstr;

// We will be using lambda in the test instead of defining a
// TransactionFunction for each of the test case.
//
// We do have a TransactionFunction-version of the test
// TestGetNonexistentDocumentThenCreate to test the non-lambda API.

class TransactionTest : public FirestoreIntegrationTest {
 protected:
#if defined(FIREBASE_USE_STD_FUNCTION)
  // We occasionally get transient error like "Could not reach Cloud Firestore
  // backend. Backend didn't respond within 10 seconds". Transaction requires
  // online and thus will not retry. So we do the retry in the testcase.
  void RunTransactionAndExpect(
      Error error, const char* message,
      std::function<Error(Transaction&, std::string&)> update) {
    Future<void> future;
    // Re-try 5 times in case server is unavailable.
    for (int i = 0; i < 5; ++i) {
      future = TestFirestore()->RunTransaction(update);
      Await(future);
      if (future.error() == Error::kErrorUnavailable) {
        std::cout << "Could not reach backend. Retrying transaction test."
                  << std::endl;
      } else {
        break;
      }
    }
    EXPECT_EQ(error, future.error());
    EXPECT_THAT(future.error_message(), HasSubstr(message));
  }

  void RunTransactionAndExpect(
      Error error, std::function<Error(Transaction&, std::string&)> update) {
    switch (error) {
      case Error::kErrorOk:
        RunTransactionAndExpect(Error::kErrorOk, "", std::move(update));
        break;
      case Error::kErrorAborted:
        RunTransactionAndExpect(
#if defined(__APPLE__)
            Error::kErrorFailedPrecondition,
#else
            Error::kErrorAborted,
#endif
            "Transaction failed all retries.", std::move(update));
        break;
      case Error::kErrorFailedPrecondition:
        // Here specifies error message of the most common cause. There are
        // other causes for FailedPrecondition as well. Use the one with message
        // parameter if the expected error message is different.
        RunTransactionAndExpect(Error::kErrorFailedPrecondition,
                                "Can't update a document that doesn't exist.",
                                std::move(update));
        break;
      default:
        FAIL() << "Unexpected error code: " << error;
    }
  }
#endif  // defined(FIREBASE_USE_STD_FUNCTION)
};

class TestTransactionFunction : public TransactionFunction {
 public:
  TestTransactionFunction(DocumentReference doc) : doc_(doc) {}

  Error Apply(Transaction& transaction, std::string& error_message) override {
    Error error = Error::kErrorUnknown;
    DocumentSnapshot snapshot = transaction.Get(doc_, &error, &error_message);
    EXPECT_EQ(Error::kErrorOk, error);
    EXPECT_FALSE(snapshot.exists());
    transaction.Set(doc_, MapFieldValue{{key_, FieldValue::String(value_)}});
    return error;
  }

  std::string key() { return key_; }
  std::string value() { return value_; }

 private:
  DocumentReference doc_;
  const std::string key_{"foo"};
  const std::string value_{"bar"};
};

TEST_F(TransactionTest, TestGetNonexistentDocumentThenCreatePortableVersion) {
  DocumentReference doc = TestFirestore()->Collection("towns").Document();
  TestTransactionFunction transaction{doc};
  Future<void> future = TestFirestore()->RunTransaction(&transaction);
  Await(future);

  EXPECT_EQ(Error::kErrorOk, future.error());
  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_EQ(FieldValue::String(transaction.value()),
            snapshot.Get(transaction.key()));
}

#if defined(FIREBASE_USE_STD_FUNCTION)

class TransactionStage {
 public:
  TransactionStage(
      std::string tag,
      std::function<void(Transaction*, const DocumentReference&)> func)
      : tag_(std::move(tag)), func_(std::move(func)) {}

  const std::string& tag() const { return tag_; }

  void operator()(Transaction* transaction,
                  const DocumentReference& doc) const {
    func_(transaction, doc);
  }

  bool operator==(const TransactionStage& rhs) const {
    return tag_ == rhs.tag_;
  }

  bool operator!=(const TransactionStage& rhs) const {
    return tag_ != rhs.tag_;
  }

 private:
  std::string tag_;
  std::function<void(Transaction*, const DocumentReference&)> func_;
};

/**
 * The transaction stages that follow are postfixed by numbers to indicate the
 * calling order. For example, calling `set1` followed by `set2` should result
 * in the document being set to the value specified by `set2`.
 */
const auto delete1 = new TransactionStage(
    "delete", [](Transaction* transaction, const DocumentReference& doc) {
      transaction->Delete(doc);
    });

const auto update1 = new TransactionStage("update", [](Transaction* transaction,
                                                       const DocumentReference&
                                                           doc) {
  transaction->Update(doc, MapFieldValue{{"foo", FieldValue::String("bar1")}});
});

const auto update2 = new TransactionStage("update", [](Transaction* transaction,
                                                       const DocumentReference&
                                                           doc) {
  transaction->Update(doc, MapFieldValue{{"foo", FieldValue::String("bar2")}});
});

const auto set1 = new TransactionStage(
    "set", [](Transaction* transaction, const DocumentReference& doc) {
      transaction->Set(doc, MapFieldValue{{"foo", FieldValue::String("bar1")}});
    });

const auto set2 = new TransactionStage(
    "set", [](Transaction* transaction, const DocumentReference& doc) {
      transaction->Set(doc, MapFieldValue{{"foo", FieldValue::String("bar2")}});
    });

const auto get = new TransactionStage(
    "get", [](Transaction* transaction, const DocumentReference& doc) {
      Error error;
      std::string msg;
      transaction->Get(doc, &error, &msg);
    });

/**
 * Used for testing that all possible combinations of executing transactions
 * result in the desired document value or error.
 *
 * `Run()`, `WithExistingDoc()`, and `WithNonexistentDoc()` don't actually do
 * anything except assign variables into the `TransactionTester`.
 *
 * `ExpectDoc()`, `ExpectNoDoc()`, and `ExpectError()` will trigger the
 * transaction to run and assert that the end result matches the input.
 */
class TransactionTester {
 public:
  explicit TransactionTester(Firestore* db) : db_(db) {}

  template <typename... Args>
  TransactionTester& Run(Args... args) {
    stages_ = {*args...};
    return *this;
  }

  TransactionTester& WithExistingDoc() {
    from_existing_doc_ = true;
    return *this;
  }

  TransactionTester& WithNonexistentDoc() {
    from_existing_doc_ = false;
    return *this;
  }

  void ExpectDoc(const MapFieldValue& expected) {
    PrepareDoc();
    RunSuccessfulTransaction();
    Future<DocumentSnapshot> future = doc_.Get();
    const DocumentSnapshot* snapshot = FirestoreIntegrationTest::Await(future);
    EXPECT_TRUE(snapshot->exists());
    EXPECT_THAT(snapshot->GetData(), expected);
    stages_.clear();
  }

  void ExpectNoDoc() {
    PrepareDoc();
    RunSuccessfulTransaction();
    Future<DocumentSnapshot> future = doc_.Get();
    const DocumentSnapshot* snapshot = FirestoreIntegrationTest::Await(future);
    EXPECT_FALSE(snapshot->exists());
    stages_.clear();
  }

  void ExpectError(Error error) {
    PrepareDoc();
    RunFailingTransaction(error);
    stages_.clear();
  }

 private:
  void PrepareDoc() {
    doc_ = db_->Collection("tx-tester").Document();
    if (from_existing_doc_) {
      FirestoreIntegrationTest::Await(
          doc_.Set(MapFieldValue{{"foo", FieldValue::String("bar0")}}));
    }
  }

  void RunSuccessfulTransaction() {
    Future<void> future = db_->RunTransaction(
        [this](Transaction& transaction, std::string& error_message) {
          for (const auto& stage : stages_) {
            stage(&transaction, doc_);
          }
          return Error::kErrorOk;
        });
    FirestoreIntegrationTest::Await(future);
    EXPECT_EQ(Error::kErrorOk, future.error())
        << "Expected the sequence (" + ListStages() + ") to succeed, but got " +
               std::to_string(future.error());
  }

  void RunFailingTransaction(Error error) {
    Future<void> future = db_->RunTransaction(
        [this](Transaction& transaction, std::string& error_message) {
          for (const auto& stage : stages_) {
            stage(&transaction, doc_);
          }
          return Error::kErrorOk;
        });
    FirestoreIntegrationTest::Await(future);
    EXPECT_EQ(error, future.error())
        << "Expected the sequence (" + ListStages() +
               ") to fail with the error " + std::to_string(error);
  }

  std::string ListStages() const {
    std::vector<std::string> stages;
    for (const auto& stage : stages_) {
      stages.push_back(stage.tag());
    }
    return absl::StrJoin(stages, ",");
  }

  Firestore* db_ = nullptr;
  DocumentReference doc_;
  bool from_existing_doc_ = false;
  std::vector<TransactionStage> stages_;
};

TEST_F(TransactionTest, TestRunsTransactionsAfterGettingNonexistentDoc) {
  SCOPED_TRACE("TestRunsTransactionsAfterGettingNonexistentDoc");

  TransactionTester tt = TransactionTester(TestFirestore());
  tt.WithExistingDoc().Run(get, delete1, delete1).ExpectNoDoc();
  tt.WithExistingDoc()
      .Run(get, delete1, update2)
      .ExpectError(Error::kErrorInvalidArgument);
  tt.WithExistingDoc()
      .Run(get, delete1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});

  tt.WithExistingDoc().Run(get, update1, delete1).ExpectNoDoc();
  tt.WithExistingDoc()
      .Run(get, update1, update2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
  tt.WithExistingDoc()
      .Run(get, update1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});

  tt.WithExistingDoc().Run(get, set1, delete1).ExpectNoDoc();
  tt.WithExistingDoc()
      .Run(get, set1, update2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
  tt.WithExistingDoc()
      .Run(get, set1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
}

TEST_F(TransactionTest, TestRunsTransactionsAfterGettingExistingDoc) {
  SCOPED_TRACE("TestRunsTransactionsAfterGettingExistingDoc");

  TransactionTester tt = TransactionTester(TestFirestore());
  tt.WithNonexistentDoc().Run(get, delete1, delete1).ExpectNoDoc();
  tt.WithNonexistentDoc()
      .Run(get, delete1, update2)
      .ExpectError(Error::kErrorInvalidArgument);
  tt.WithNonexistentDoc()
      .Run(get, delete1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});

  tt.WithNonexistentDoc()
      .Run(get, update1, delete1)
      .ExpectError(Error::kErrorInvalidArgument);
  tt.WithNonexistentDoc()
      .Run(get, update1, update2)
      .ExpectError(Error::kErrorInvalidArgument);
  tt.WithNonexistentDoc()
      .Run(get, update1, set2)
      .ExpectError(Error::kErrorInvalidArgument);

  tt.WithNonexistentDoc().Run(get, set1, delete1).ExpectNoDoc();
  tt.WithNonexistentDoc()
      .Run(get, set1, update2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
  tt.WithNonexistentDoc()
      .Run(get, set1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
}

TEST_F(TransactionTest, TestRunsTransactionsOnExistingDoc) {
  SCOPED_TRACE("TestRunTransactionsOnExistingDoc");

  TransactionTester tt = TransactionTester(TestFirestore());
  tt.WithExistingDoc().Run(delete1, delete1).ExpectNoDoc();
  tt.WithExistingDoc()
      .Run(delete1, update2)
      .ExpectError(Error::kErrorInvalidArgument);
  tt.WithExistingDoc()
      .Run(get, delete1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});

  tt.WithExistingDoc().Run(update1, delete1).ExpectNoDoc();
  tt.WithExistingDoc()
      .Run(update1, update2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
  tt.WithExistingDoc()
      .Run(update1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});

  tt.WithExistingDoc().Run(set1, delete1).ExpectNoDoc();
  tt.WithExistingDoc()
      .Run(set1, update2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
  tt.WithExistingDoc()
      .Run(set1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
}

TEST_F(TransactionTest, TestRunsTransactionsOnNonexistentDoc) {
  SCOPED_TRACE("TestRunsTransactionsOnNonexistentDoc");

  TransactionTester tt = TransactionTester(TestFirestore());
  tt.WithNonexistentDoc().Run(delete1, delete1).ExpectNoDoc();
  tt.WithNonexistentDoc()
      .Run(delete1, update2)
      .ExpectError(Error::kErrorInvalidArgument);
  tt.WithNonexistentDoc()
      .Run(delete1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});

  tt.WithNonexistentDoc()
      .Run(update1, delete1)
      .ExpectError(Error::kErrorNotFound);
  tt.WithNonexistentDoc()
      .Run(update1, update2)
      .ExpectError(Error::kErrorNotFound);
  tt.WithNonexistentDoc().Run(update1, set2).ExpectError(Error::kErrorNotFound);

  tt.WithNonexistentDoc().Run(set1, delete1).ExpectNoDoc();
  tt.WithNonexistentDoc()
      .Run(set1, update2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
  tt.WithNonexistentDoc()
      .Run(set1, set2)
      .ExpectDoc(MapFieldValue{{"foo", FieldValue::String("bar2")}});
}

TEST_F(TransactionTest, TestGetNonexistentDocumentThenFailPatch) {
  DocumentReference doc = TestFirestore()->Collection("towns").Document();

  SCOPED_TRACE("TestGetNonexistentDocumentThenFailPatch");
  RunTransactionAndExpect(
      Error::kErrorInvalidArgument,
      "Can't update a document that doesn't exist.",
      [doc](Transaction& transaction, std::string& error_message) -> Error {
        Error error = Error::kErrorOk;
        DocumentSnapshot snapshot =
            transaction.Get(doc, &error, &error_message);
        EXPECT_EQ(Error::kErrorOk, error);
        EXPECT_FALSE(snapshot.exists());
        transaction.Update(doc,
                           MapFieldValue{{"foo", FieldValue::String("bar")}});
        return error;
      });
}

TEST_F(TransactionTest, TestSetDocumentWithMerge) {
  DocumentReference doc = TestFirestore()->Collection("towns").Document();

  SCOPED_TRACE("TestSetDocumentWithMerge");
  RunTransactionAndExpect(
      Error::kErrorOk,
      [doc](Transaction& transaction, std::string& error_message) -> Error {
        transaction.Set(
            doc,
            MapFieldValue{{"a", FieldValue::String("b")},
                          {"nested", FieldValue::Map(MapFieldValue{
                                         {"a", FieldValue::String("b")}})}});
        transaction.Set(
            doc,
            MapFieldValue{{"c", FieldValue::String("d")},
                          {"nested", FieldValue::Map(MapFieldValue{
                                         {"c", FieldValue::String("d")}})}},
            SetOptions::Merge());
        return Error::kErrorOk;
      });

  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_THAT(snapshot.GetData(),
              testing::ContainerEq(MapFieldValue{
                  {"a", FieldValue::String("b")},
                  {"c", FieldValue::String("d")},
                  {"nested", FieldValue::Map(MapFieldValue{
                                 {"a", FieldValue::String("b")},
                                 {"c", FieldValue::String("d")}})}}));
}

TEST_F(TransactionTest, TestCannotUpdateNonExistentDocument) {
  DocumentReference doc = TestFirestore()->Collection("towns").Document();

  SCOPED_TRACE("TestCannotUpdateNonExistentDocument");
  RunTransactionAndExpect(
      Error::kErrorNotFound, "",
      [doc](Transaction& transaction, std::string& error_message) -> Error {
        transaction.Update(doc,
                           MapFieldValue{{"foo", FieldValue::String("bar")}});
        return Error::kErrorOk;
      });

  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_FALSE(snapshot.exists());
}

TEST_F(TransactionTest, TestIncrementTransactionally) {
  // A set of concurrent transactions.
  std::vector<Future<void>> transaction_tasks;
  // A barrier to make sure every transaction reaches the same spot.
  Semaphore write_barrier{0};
  // Use these two as a portable way to mimic atomic integer.
  Mutex started_locker;
  int started = 0;

  DocumentReference doc = TestFirestore()->Collection("counters").Document();
  WriteDocument(doc, MapFieldValue{{"count", FieldValue::Double(5.0)}});

  // Make 3 transactions that will all increment.
  // Note: Visual Studio 2015 incorrectly requires `kTotal` to be captured in
  // the lambda, even though it's a constant expression. Adding `static` as
  // a workaround.
  static constexpr int kTotal = 3;
  for (int i = 0; i < kTotal; ++i) {
    transaction_tasks.push_back(TestFirestore()->RunTransaction(
        [doc, &write_barrier, &started_locker, &started](
            Transaction& transaction, std::string& error_message) -> Error {
          Error error = Error::kErrorOk;
          DocumentSnapshot snapshot =
              transaction.Get(doc, &error, &error_message);
          EXPECT_EQ(Error::kErrorOk, error);
          {
            MutexLock lock(started_locker);
            ++started;
            // Once all of the transactions have read, allow the first write.
            if (started == kTotal) {
              write_barrier.Post();
            }
          }

          // Let all of the transactions fetch the old value and stop once.
          write_barrier.Wait();
          // Refill the barrier so that the other transactions and retries
          // succeed.
          write_barrier.Post();

          double new_count = snapshot.Get("count").double_value() + 1.0;
          transaction.Set(
              doc, MapFieldValue{{"count", FieldValue::Double(new_count)}});
          return error;
        }));
  }

  // Until we have another Await() that waits for multiple Futures, we do the
  // wait in one by one.
  while (!transaction_tasks.empty()) {
    Future<void> future = transaction_tasks.back();
    transaction_tasks.pop_back();
    Await(future);
    EXPECT_EQ(Error::kErrorOk, future.error());
  }
  // Now all transaction should be completed, so check the result.
  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_DOUBLE_EQ(5.0 + kTotal, snapshot.Get("count").double_value());
}

TEST_F(TransactionTest, TestUpdateTransactionally) {
  // A set of concurrent transactions.
  std::vector<Future<void>> transaction_tasks;
  // A barrier to make sure every transaction reaches the same spot.
  Semaphore write_barrier{0};
  // Use these two as a portable way to mimic atomic integer.
  Mutex started_locker;
  int started = 0;

  DocumentReference doc = TestFirestore()->Collection("counters").Document();
  WriteDocument(doc, MapFieldValue{{"count", FieldValue::Double(5.0)},
                                   {"other", FieldValue::String("yes")}});

  // Make 3 transactions that will all increment.
  static const constexpr int kTotal = 3;
  for (int i = 0; i < kTotal; ++i) {
    transaction_tasks.push_back(TestFirestore()->RunTransaction(
        [doc, &write_barrier, &started_locker, &started](
            Transaction& transaction, std::string& error_message) -> Error {
          Error error = Error::kErrorOk;
          DocumentSnapshot snapshot =
              transaction.Get(doc, &error, &error_message);
          EXPECT_EQ(Error::kErrorOk, error);
          {
            MutexLock lock(started_locker);
            ++started;
            // Once all of the transactions have read, allow the first write.
            if (started == kTotal) {
              write_barrier.Post();
            }
          }

          // Let all of the transactions fetch the old value and stop once.
          write_barrier.Wait();
          // Refill the barrier so that the other transactions and retries
          // succeed.
          write_barrier.Post();

          double new_count = snapshot.Get("count").double_value() + 1.0;
          transaction.Update(
              doc, MapFieldValue{{"count", FieldValue::Double(new_count)}});
          return error;
        }));
  }

  // Until we have another Await() that waits for multiple Futures, we do the
  // wait in backward order.
  while (!transaction_tasks.empty()) {
    Future<void> future = transaction_tasks.back();
    transaction_tasks.pop_back();
    Await(future);
    EXPECT_EQ(Error::kErrorOk, future.error());
  }
  // Now all transaction should be completed, so check the result.
  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_DOUBLE_EQ(5.0 + kTotal, snapshot.Get("count").double_value());
  EXPECT_EQ("yes", snapshot.Get("other").string_value());
}

TEST_F(TransactionTest, TestUpdateFieldsWithDotsTransactionally) {
  DocumentReference doc = TestFirestore()->Collection("fieldnames").Document();
  WriteDocument(doc, MapFieldValue{{"a.b", FieldValue::String("old")},
                                   {"c.d", FieldValue::String("old")},
                                   {"e.f", FieldValue::String("old")}});

  SCOPED_TRACE("TestUpdateFieldsWithDotsTransactionally");
  RunTransactionAndExpect(
      Error::kErrorOk,
      [doc](Transaction& transaction, std::string& error_message) -> Error {
        transaction.Update(doc, MapFieldPathValue{{FieldPath{"a.b"},
                                                   FieldValue::String("new")}});
        transaction.Update(doc, MapFieldPathValue{{FieldPath{"c.d"},
                                                   FieldValue::String("new")}});
        return Error::kErrorOk;
      });

  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_THAT(snapshot.GetData(), testing::ContainerEq(MapFieldValue{
                                      {"a.b", FieldValue::String("new")},
                                      {"c.d", FieldValue::String("new")},
                                      {"e.f", FieldValue::String("old")}}));
}

TEST_F(TransactionTest, TestUpdateNestedFieldsTransactionally) {
  DocumentReference doc = TestFirestore()->Collection("fieldnames").Document();
  WriteDocument(
      doc, MapFieldValue{
               {"a", FieldValue::Map({{"b", FieldValue::String("old")}})},
               {"c", FieldValue::Map({{"d", FieldValue::String("old")}})},
               {"e", FieldValue::Map({{"f", FieldValue::String("old")}})}});

  SCOPED_TRACE("TestUpdateNestedFieldsTransactionally");
  RunTransactionAndExpect(
      Error::kErrorOk,
      [doc](Transaction& transaction, std::string& error_message) -> Error {
        transaction.Update(doc,
                           MapFieldValue{{"a.b", FieldValue::String("new")}});
        transaction.Update(doc,
                           MapFieldValue{{"c.d", FieldValue::String("new")}});
        return Error::kErrorOk;
      });

  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_THAT(snapshot.GetData(),
              testing::ContainerEq(MapFieldValue{
                  {"a", FieldValue::Map({{"b", FieldValue::String("new")}})},
                  {"c", FieldValue::Map({{"d", FieldValue::String("new")}})},
                  {"e", FieldValue::Map({{"f", FieldValue::String("old")}})}}));
}

#if defined(__ANDROID__)
// TODO(b/136012313): on iOS, this triggers assertion failure.
TEST_F(TransactionTest, TestCannotReadAfterWriting) {
  DocumentReference doc = TestFirestore()->Collection("anything").Document();
  DocumentSnapshot snapshot;

  SCOPED_TRACE("TestCannotReadAfterWriting");
  RunTransactionAndExpect(
      Error::kErrorInvalidArgument,
      "Firestore transactions require all reads to be "
      "executed before all writes.",
      [doc, &snapshot](Transaction& transaction,
                       std::string& error_message) -> Error {
        Error error = Error::kErrorOk;
        transaction.Set(doc, MapFieldValue{{"foo", FieldValue::String("bar")}});
        snapshot = transaction.Get(doc, &error, &error_message);
        return error;
      });

  snapshot = ReadDocument(doc);
  EXPECT_FALSE(snapshot.exists());
}
#endif

TEST_F(TransactionTest, TestCanHaveGetsWithoutMutations) {
  DocumentReference doc1 = TestFirestore()->Collection("foo").Document();
  DocumentReference doc2 = TestFirestore()->Collection("foo").Document();
  WriteDocument(doc1, MapFieldValue{{"foo", FieldValue::String("bar")}});
  DocumentSnapshot snapshot;

  SCOPED_TRACE("TestCanHaveGetsWithoutMutations");
  RunTransactionAndExpect(
      Error::kErrorOk,
      [doc1, doc2, &snapshot](Transaction& transaction,
                              std::string& error_message) -> Error {
        Error error = Error::kErrorOk;
        transaction.Get(doc2, &error, &error_message);
        EXPECT_EQ(Error::kErrorOk, error);
        snapshot = transaction.Get(doc1, &error, &error_message);
        EXPECT_EQ(Error::kErrorOk, error);
        return error;
      });
  EXPECT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      testing::ContainerEq(MapFieldValue{{"foo", FieldValue::String("bar")}}));
}

TEST_F(TransactionTest, TestSuccessWithNoTransactionOperations) {
  SCOPED_TRACE("TestSuccessWithNoTransactionOperations");
  RunTransactionAndExpect(
      Error::kErrorOk,
      [](Transaction&, std::string&) -> Error { return Error::kErrorOk; });
}

TEST_F(TransactionTest, TestCancellationOnError) {
  DocumentReference doc = TestFirestore()->Collection("towns").Document();
  // Use these two as a portable way to mimic atomic integer.
  Mutex count_locker;
  int count = 0;

  SCOPED_TRACE("TestCancellationOnError");
  RunTransactionAndExpect(
      Error::kErrorDeadlineExceeded, "no",
      [doc, &count_locker, &count](Transaction& transaction,
                                   std::string& error_message) -> Error {
        {
          MutexLock lock{count_locker};
          ++count;
        }
        transaction.Set(doc, MapFieldValue{{"foo", FieldValue::String("bar")}});
        error_message = "no";
        return Error::kErrorDeadlineExceeded;
      });

  // TODO(varconst): uncomment. Currently, there is no way in C++ to distinguish
  // user error, so the transaction gets retried, and the counter goes up to 6.
  // EXPECT_EQ(1, count);
  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_FALSE(snapshot.exists());
}

#endif  // defined(FIREBASE_USE_STD_FUNCTION)

#endif  // defined(__ANDROID__) || defined(__APPLE__)

}  // namespace firestore
}  // namespace firebase
