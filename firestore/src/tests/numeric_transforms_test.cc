#include <string>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

class NumericTransformsTest : public FirestoreIntegrationTest {
 public:
  NumericTransformsTest() {
    doc_ref_ = Document();
    listener_ =
        accumulator_.listener()->AttachTo(&doc_ref_, MetadataChanges::kInclude);

    // Wait for initial null snapshot to avoid potential races.
    DocumentSnapshot initial_snapshot = accumulator_.AwaitRemoteEvent();
    EXPECT_FALSE(initial_snapshot.exists());
  }

  ~NumericTransformsTest() override { listener_.Remove(); }

 protected:
  /** Writes values and waits for the corresponding snapshot. */
  void WriteInitialData(const MapFieldValue& doc) {
    WriteDocument(doc_ref_, doc);

    accumulator_.AwaitRemoteEvent();
  }

  void ExpectLocalAndRemoteValue(int value) {
    DocumentSnapshot snap = accumulator_.AwaitLocalEvent();
    EXPECT_TRUE(snap.Get("sum").is_integer());
    EXPECT_EQ(value, snap.Get("sum").integer_value());
    snap = accumulator_.AwaitRemoteEvent();
    EXPECT_TRUE(snap.Get("sum").is_integer());
    EXPECT_EQ(value, snap.Get("sum").integer_value());
  }

  void ExpectLocalAndRemoteValue(double value) {
    DocumentSnapshot snap = accumulator_.AwaitLocalEvent();
    EXPECT_TRUE(snap.Get("sum").is_double());
    EXPECT_DOUBLE_EQ(value, snap.Get("sum").double_value());
    snap = accumulator_.AwaitRemoteEvent();
    EXPECT_TRUE(snap.Get("sum").is_double());
    EXPECT_DOUBLE_EQ(value, snap.Get("sum").double_value());
  }

  // A document reference to read and write.
  DocumentReference doc_ref_;

  // Accumulator used to capture events during the test.
  EventAccumulator<DocumentSnapshot> accumulator_;

  // Listener registration for a listener maintained during the course of the
  // test.
  ListenerRegistration listener_;
};

TEST_F(NumericTransformsTest, CreateDocumentWithIncrement) {
  Await(doc_ref_.Set({{"sum", FieldValue::Increment(1337)}}));

  ExpectLocalAndRemoteValue(1337);
}

TEST_F(NumericTransformsTest, MergeOnNonExistingDocumentWithIncrement) {
  MapFieldValue data = {{"sum", FieldValue::Integer(1337)}};

  Await(doc_ref_.Set(data, SetOptions::Merge()));

  ExpectLocalAndRemoteValue(1337);
}

TEST_F(NumericTransformsTest, IntegerIncrementWithExistingInteger) {
  WriteInitialData({{"sum", FieldValue::Integer(1337)}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(1)}}));

  ExpectLocalAndRemoteValue(1338);
}

TEST_F(NumericTransformsTest, DoubleIncrementWithExistingDouble) {
  WriteInitialData({{"sum", FieldValue::Double(13.37)}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(0.1)}}));

  ExpectLocalAndRemoteValue(13.47);
}

TEST_F(NumericTransformsTest, IntegerIncrementWithExistingDouble) {
  WriteInitialData({{"sum", FieldValue::Double(13.37)}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(1)}}));

  ExpectLocalAndRemoteValue(14.37);
}

TEST_F(NumericTransformsTest, DoubleIncrementWithExistingInteger) {
  WriteInitialData({{"sum", FieldValue::Integer(1337)}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(0.1)}}));

  ExpectLocalAndRemoteValue(1337.1);
}

TEST_F(NumericTransformsTest, IntegerIncrementWithExistingString) {
  WriteInitialData({{"sum", FieldValue::String("overwrite")}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(1337)}}));

  ExpectLocalAndRemoteValue(1337);
}

TEST_F(NumericTransformsTest, DoubleIncrementWithExistingString) {
  WriteInitialData({{"sum", FieldValue::String("overwrite")}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(13.37)}}));

  ExpectLocalAndRemoteValue(13.37);
}

TEST_F(NumericTransformsTest, MultipleDoubleIncrements) {
  WriteInitialData({{"sum", FieldValue::Double(0.0)}});

  DisableNetwork();

  doc_ref_.Update({{"sum", FieldValue::Increment(0.1)}});
  doc_ref_.Update({{"sum", FieldValue::Increment(0.01)}});
  doc_ref_.Update({{"sum", FieldValue::Increment(0.001)}});

  DocumentSnapshot snap = accumulator_.AwaitLocalEvent();

  EXPECT_TRUE(snap.Get("sum").is_double());
  EXPECT_DOUBLE_EQ(0.1, snap.Get("sum").double_value());

  snap = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(snap.Get("sum").is_double());
  EXPECT_DOUBLE_EQ(0.11, snap.Get("sum").double_value());

  snap = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(snap.Get("sum").is_double());
  EXPECT_DOUBLE_EQ(0.111, snap.Get("sum").double_value());

  EnableNetwork();

  snap = accumulator_.AwaitRemoteEvent();
  EXPECT_DOUBLE_EQ(0.111, snap.Get("sum").double_value());
}

TEST_F(NumericTransformsTest, IncrementTwiceInABatch) {
  WriteInitialData({{"sum", FieldValue::String("overwrite")}});

  WriteBatch batch = firestore()->batch();

  batch.Update(doc_ref_, {{"sum", FieldValue::Increment(1)}});
  batch.Update(doc_ref_, {{"sum", FieldValue::Increment(1)}});

  Await(batch.Commit());

  ExpectLocalAndRemoteValue(2);
}

TEST_F(NumericTransformsTest, IncrementDeleteIncrementInABatch) {
  WriteInitialData({{"sum", FieldValue::String("overwrite")}});

  WriteBatch batch = firestore()->batch();

  batch.Update(doc_ref_, {{"sum", FieldValue::Increment(1)}});
  batch.Update(doc_ref_, {{"sum", FieldValue::Delete()}});
  batch.Update(doc_ref_, {{"sum", FieldValue::Increment(3)}});

  Await(batch.Commit());

  ExpectLocalAndRemoteValue(3);
}

TEST_F(NumericTransformsTest, ServerTimestampAndIncrement) {
  DisableNetwork();

  doc_ref_.Set({{"sum", FieldValue::ServerTimestamp()}});
  doc_ref_.Set({{"sum", FieldValue::Increment(1)}});

  DocumentSnapshot snapshot = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(
      snapshot.Get("sum", ServerTimestampBehavior::kEstimate).is_timestamp());

  DocumentSnapshot snap = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(snap.Get("sum").is_integer());
  EXPECT_EQ(1, snap.Get("sum").integer_value());

  EnableNetwork();

  snap = accumulator_.AwaitRemoteEvent();
  EXPECT_TRUE(snap.Get("sum").is_integer());
  EXPECT_EQ(1, snap.Get("sum").integer_value());
}

}  // namespace firestore
}  // namespace firebase
