#include <cstdlib>
#include <string>
#include <vector>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRServerTimestampTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/ServerTimestampTest.java

namespace firebase {
namespace firestore {

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

class ServerTimestampTest : public FirestoreIntegrationTest {
 public:
  ~ServerTimestampTest() override {}

 protected:
  void SetUp() override {
    doc_ = Document();
    listener_registration_ =
        accumulator_.listener()->AttachTo(&doc_, MetadataChanges::kInclude);

    // Wait for initial null snapshot to avoid potential races.
    DocumentSnapshot initial_snapshot = accumulator_.AwaitServerEvent();
    EXPECT_FALSE(initial_snapshot.exists());
  }

  void TearDown() override { listener_registration_.Remove(); }

  /** Returns the expected data, with the specified timestamp substituted in. */
  MapFieldValue ExpectedDataWithTimestamp(const FieldValue& timestamp) {
    return MapFieldValue{{"a", FieldValue::Integer(42)},
                         {"when", timestamp},
                         {"deep", FieldValue::Map({{"when", timestamp}})}};
  }

  /** Writes initial_data_ and waits for the corresponding snapshot. */
  void WriteInitialData() {
    WriteDocument(doc_, initial_data_);
    DocumentSnapshot initial_data_snapshot = accumulator_.Await();
    EXPECT_THAT(initial_data_snapshot.GetData(),
                testing::ContainerEq(initial_data_));
    initial_data_snapshot = accumulator_.Await();
    EXPECT_THAT(initial_data_snapshot.GetData(),
                testing::ContainerEq(initial_data_));
  }

  /**
   * Verifies a snapshot containing set_data_ but with null for the timestamps.
   */
  void VerifyTimestampsAreNull(const DocumentSnapshot& snapshot) {
    EXPECT_THAT(
        snapshot.GetData(),
        testing::ContainerEq(ExpectedDataWithTimestamp(FieldValue::Null())));
  }

  /**
   * Verifies a snapshot containing set_data_ but with resolved server
   * timestamps.
   */
  void VerifyTimestampsAreResolved(const DocumentSnapshot& snapshot) {
    ASSERT_TRUE(snapshot.exists());
    ASSERT_TRUE(snapshot.Get("when").is_timestamp());
    Timestamp when = snapshot.Get("when").timestamp_value();
    // Tolerate up to 48*60*60 seconds of clock skew between client and server.
    // This should be more than enough to compensate for timezone issues (even
    // after taking daylight saving into account) and should allow local clocks
    // to deviate from true time slightly and still pass the test. PORT_NOTE:
    // For the tolerance here, Android uses 48*60*60 seconds while iOS uses 10
    // seconds.
    int delta_sec = 48 * 60 * 60;
    Timestamp now = Timestamp::Now();
    EXPECT_LT(abs(when.seconds() - now.seconds()), delta_sec)
        << "resolved timestamp (" << when.ToString() << ") should be within "
        << delta_sec << "s of now (" << now.ToString() << ")";

    // Validate the rest of the document.
    EXPECT_THAT(snapshot.GetData(),
                testing::ContainerEq(
                    ExpectedDataWithTimestamp(FieldValue::Timestamp(when))));
  }

  /**
   * Verifies a snapshot containing set_data_ but with local estimates for
   * server timestamps.
   */
  void VerifyTimestampsAreEstimates(const DocumentSnapshot& snapshot) {
    ASSERT_TRUE(snapshot.exists());
    FieldValue when = snapshot.Get("when", ServerTimestampBehavior::kEstimate);
    ASSERT_TRUE(when.is_timestamp());
    EXPECT_THAT(snapshot.GetData(ServerTimestampBehavior::kEstimate),
                testing::ContainerEq(ExpectedDataWithTimestamp(when)));
  }

  /**
   * Verifies a snapshot containing set_data_ but using the previous field value
   * for server timestamps.
   */
  void VerifyTimestampsUsePreviousValue(const DocumentSnapshot& snapshot,
                                        const FieldValue& previous) {
    ASSERT_TRUE(snapshot.exists());
    ASSERT_TRUE(previous.is_null() || previous.is_timestamp());
    EXPECT_THAT(snapshot.GetData(ServerTimestampBehavior::kPrevious),
                testing::ContainerEq(ExpectedDataWithTimestamp(previous)));
  }

  // Data written in tests via set.
  const MapFieldValue set_data_ = MapFieldValue{
      {"a", FieldValue::Integer(42)},
      {"when", FieldValue::ServerTimestamp()},
      {"deep", FieldValue::Map({{"when", FieldValue::ServerTimestamp()}})}};

  // Base and update data used for update tests.
  const MapFieldValue initial_data_ =
      MapFieldValue{{"a", FieldValue::Integer(42)}};
  const MapFieldValue update_data_ = MapFieldValue{
      {"when", FieldValue::ServerTimestamp()},
      {"deep", FieldValue::Map({{"when", FieldValue::ServerTimestamp()}})}};

  // A document reference to read and write to.
  DocumentReference doc_;

  // Accumulator used to capture events during the test.
  EventAccumulator<DocumentSnapshot> accumulator_;

  // Listener registration for a listener maintained during the course of the
  // test.
  ListenerRegistration listener_registration_;
};

TEST_F(ServerTimestampTest, TestServerTimestampsWorkViaSet) {
  WriteDocument(doc_, set_data_);
  VerifyTimestampsAreNull(accumulator_.AwaitLocalEvent());
  VerifyTimestampsAreResolved(accumulator_.AwaitRemoteEvent());
}

TEST_F(ServerTimestampTest, TestServerTimestampsWorkViaUpdate) {
  WriteInitialData();
  UpdateDocument(doc_, update_data_);
  VerifyTimestampsAreNull(accumulator_.AwaitLocalEvent());
  VerifyTimestampsAreResolved(accumulator_.AwaitRemoteEvent());
}

TEST_F(ServerTimestampTest, TestServerTimestampsCanReturnEstimatedValue) {
  WriteDocument(doc_, set_data_);
  VerifyTimestampsAreEstimates(accumulator_.AwaitLocalEvent());
  VerifyTimestampsAreResolved(accumulator_.AwaitRemoteEvent());
}

TEST_F(ServerTimestampTest, TestServerTimestampsCanReturnPreviousValue) {
  WriteDocument(doc_, set_data_);
  VerifyTimestampsUsePreviousValue(accumulator_.AwaitLocalEvent(),
                                   FieldValue::Null());
  DocumentSnapshot previous_snapshot = accumulator_.AwaitRemoteEvent();
  VerifyTimestampsAreResolved(previous_snapshot);

  UpdateDocument(doc_, update_data_);
  VerifyTimestampsUsePreviousValue(accumulator_.AwaitLocalEvent(),
                                   previous_snapshot.Get("when"));
  VerifyTimestampsAreResolved(accumulator_.AwaitRemoteEvent());
}

TEST_F(ServerTimestampTest,
       TestServerTimestampsCanReturnPreviousValueOfDifferentType) {
  WriteInitialData();
  UpdateDocument(doc_, MapFieldValue{{"a", FieldValue::ServerTimestamp()}});

  DocumentSnapshot local_snapshot = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(local_snapshot.Get("a").is_null());
  EXPECT_TRUE(local_snapshot.Get("a", ServerTimestampBehavior::kEstimate)
                  .is_timestamp());
  EXPECT_TRUE(
      local_snapshot.Get("a", ServerTimestampBehavior::kPrevious).is_integer());
  EXPECT_EQ(42, local_snapshot.Get("a", ServerTimestampBehavior::kPrevious)
                    .integer_value());

  DocumentSnapshot remote_snapshot = accumulator_.AwaitRemoteEvent();
  EXPECT_TRUE(remote_snapshot.Get("a").is_timestamp());
  EXPECT_TRUE(remote_snapshot.Get("a", ServerTimestampBehavior::kEstimate)
                  .is_timestamp());
  EXPECT_TRUE(remote_snapshot.Get("a", ServerTimestampBehavior::kPrevious)
                  .is_timestamp());
}

TEST_F(ServerTimestampTest,
       TestServerTimestampsCanRetainPreviousValueThroughConsecutiveUpdates) {
  WriteInitialData();
  Await(TestFirestore()->DisableNetwork());
  accumulator_.AwaitRemoteEvent();

  doc_.Update(MapFieldValue{{"a", FieldValue::ServerTimestamp()}});
  DocumentSnapshot local_snapshot = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(
      local_snapshot.Get("a", ServerTimestampBehavior::kPrevious).is_integer());
  EXPECT_EQ(42, local_snapshot.Get("a", ServerTimestampBehavior::kPrevious)
                    .integer_value());

  doc_.Update(MapFieldValue{{"a", FieldValue::ServerTimestamp()}});
  local_snapshot = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(
      local_snapshot.Get("a", ServerTimestampBehavior::kPrevious).is_integer());
  EXPECT_EQ(42, local_snapshot.Get("a", ServerTimestampBehavior::kPrevious)
                    .integer_value());

  Await(TestFirestore()->EnableNetwork());

  DocumentSnapshot remote_snapshot = accumulator_.AwaitRemoteEvent();
  EXPECT_TRUE(remote_snapshot.Get("a").is_timestamp());
}

TEST_F(ServerTimestampTest,
       TestServerTimestampsUsesPreviousValueFromLocalMutation) {
  WriteInitialData();
  Await(TestFirestore()->DisableNetwork());
  accumulator_.AwaitRemoteEvent();

  doc_.Update(MapFieldValue{{"a", FieldValue::ServerTimestamp()}});
  DocumentSnapshot local_snapshot = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(
      local_snapshot.Get("a", ServerTimestampBehavior::kPrevious).is_integer());
  EXPECT_EQ(42, local_snapshot.Get("a", ServerTimestampBehavior::kPrevious)
                    .integer_value());

  doc_.Update(MapFieldValue{{"a", FieldValue::Integer(1337)}});
  accumulator_.AwaitLocalEvent();

  doc_.Update(MapFieldValue{{"a", FieldValue::ServerTimestamp()}});
  local_snapshot = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(
      local_snapshot.Get("a", ServerTimestampBehavior::kPrevious).is_integer());
  EXPECT_EQ(1337, local_snapshot.Get("a", ServerTimestampBehavior::kPrevious)
                      .integer_value());

  Await(TestFirestore()->EnableNetwork());

  DocumentSnapshot remote_snapshot = accumulator_.AwaitRemoteEvent();
  EXPECT_TRUE(remote_snapshot.Get("a").is_timestamp());
}

TEST_F(ServerTimestampTest, TestServerTimestampsWorkViaTransactionSet) {
#if defined(FIREBASE_USE_STD_FUNCTION)
  Await(TestFirestore()->RunTransaction(
      [this](Transaction& transaction, std::string&) -> Error {
        transaction.Set(doc_, set_data_);
        return Error::kErrorOk;
      }));
  VerifyTimestampsAreResolved(accumulator_.AwaitRemoteEvent());
#endif  // defined(FIREBASE_USE_STD_FUNCTION)
}

TEST_F(ServerTimestampTest, TestServerTimestampsWorkViaTransactionUpdate) {
#if defined(FIREBASE_USE_STD_FUNCTION)
  WriteInitialData();
  Await(TestFirestore()->RunTransaction(
      [this](Transaction& transaction, std::string&) -> Error {
        transaction.Update(doc_, update_data_);
        return Error::kErrorOk;
      }));
  VerifyTimestampsAreResolved(accumulator_.AwaitRemoteEvent());
#endif  // defined(FIREBASE_USE_STD_FUNCTION)
}

TEST_F(ServerTimestampTest,
       TestServerTimestampsFailViaTransactionUpdateOnNonexistentDocument) {
#if defined(FIREBASE_USE_STD_FUNCTION)
  Future<void> future = TestFirestore()->RunTransaction(
      [this](Transaction& transaction, std::string&) -> Error {
        transaction.Update(doc_, update_data_);
        return Error::kErrorOk;
      });
  Await(future);
  EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
  EXPECT_EQ(Error::kErrorNotFound, future.error());
#endif  // defined(FIREBASE_USE_STD_FUNCTION)
}

TEST_F(ServerTimestampTest,
       TestServerTimestampsFailViaUpdateOnNonexistentDocument) {
  Future<void> future = doc_.Update(update_data_);
  Await(future);
  EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
  EXPECT_EQ(Error::kErrorNotFound, future.error());
}

}  // namespace firestore
}  // namespace firebase
