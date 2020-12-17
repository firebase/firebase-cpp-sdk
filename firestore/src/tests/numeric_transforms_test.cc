#include <string>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using Type = FieldValue::Type;

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

const char* TypeName(Type type) {
  switch (type) {
    case Type::kNull:
      return "kNull";
    case Type::kBoolean:
      return "kBoolean";
    case Type::kInteger:
      return "kInteger";
    case Type::kDouble:
      return "kDouble";
    case Type::kTimestamp:
      return "kTimestamp";
    case Type::kString:
      return "kString";
    case Type::kBlob:
      return "kBlob";
    case Type::kReference:
      return "kReference";
    case Type::kGeoPoint:
      return "kGeoPoint";
    case Type::kArray:
      return "kArray";
    case Type::kMap:
      return "kMap";
    case Type::kDelete:
      return "kDelete";
    case Type::kServerTimestamp:
      return "kServerTimestamp";
    case Type::kArrayUnion:
      return "kArrayUnion";
    case Type::kArrayRemove:
      return "kArrayRemove";
    case Type::kIncrementInteger:
      return "kIncrementInteger";
    case Type::kIncrementDouble:
      return "kIncrementDouble";
  }
}

void PrintTo(const Type& type, std::ostream* os) {
  *os << "Type::" << TypeName(type);
}

void PrintTo(const FieldValue& f, std::ostream* os) {
  *os << f.ToString() << " (";
  PrintTo(f.type(), os);
  *os << ")";
}

class NumericTransformsTest : public FirestoreIntegrationTest {
 public:
  NumericTransformsTest() {
    doc_ref_ = Document();
    listener_ =
        accumulator_.listener()->AttachTo(&doc_ref_, MetadataChanges::kInclude);

    // Wait for initial null snapshot to avoid potential races.
    DocumentSnapshot initial_snapshot = accumulator_.AwaitServerEvent();
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
    ASSERT_EQ(snap.Get("sum"), FieldValue::Integer(value));
    snap = accumulator_.AwaitRemoteEvent();
    ASSERT_EQ(snap.Get("sum"), FieldValue::Integer(value));
  }

  void ExpectLocalAndRemoteValue(double value) {
    DocumentSnapshot snap = accumulator_.AwaitLocalEvent();
    ASSERT_EQ(snap.Get("sum"), FieldValue::Double(value));
    snap = accumulator_.AwaitRemoteEvent();
    ASSERT_EQ(snap.Get("sum"), FieldValue::Double(value));
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
  WriteInitialData({{"sum", FieldValue::Double(0.5)}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(0.25)}}));

  ExpectLocalAndRemoteValue(0.75);
}

TEST_F(NumericTransformsTest, IntegerIncrementWithExistingDouble) {
  WriteInitialData({{"sum", FieldValue::Double(0.5)}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(1)}}));

  ExpectLocalAndRemoteValue(1.5);
}

TEST_F(NumericTransformsTest, DoubleIncrementWithExistingInteger) {
  WriteInitialData({{"sum", FieldValue::Integer(1)}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(0.5)}}));

  ExpectLocalAndRemoteValue(1.5);
}

TEST_F(NumericTransformsTest, IntegerIncrementWithExistingString) {
  WriteInitialData({{"sum", FieldValue::String("overwrite")}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(1337)}}));

  ExpectLocalAndRemoteValue(1337);
}

TEST_F(NumericTransformsTest, DoubleIncrementWithExistingString) {
  WriteInitialData({{"sum", FieldValue::String("overwrite")}});

  Await(doc_ref_.Update({{"sum", FieldValue::Increment(1.5)}}));

  ExpectLocalAndRemoteValue(1.5);
}

TEST_F(NumericTransformsTest, MultipleDoubleIncrements) {
  WriteInitialData({{"sum", FieldValue::Double(0.0)}});

  DisableNetwork();

  doc_ref_.Update({{"sum", FieldValue::Increment(0.5)}});
  doc_ref_.Update({{"sum", FieldValue::Increment(1.0)}});
  doc_ref_.Update({{"sum", FieldValue::Increment(2.0)}});

  DocumentSnapshot snap = accumulator_.AwaitLocalEvent();

  EXPECT_EQ(snap.Get("sum"), FieldValue::Double(0.5));

  snap = accumulator_.AwaitLocalEvent();
  EXPECT_EQ(snap.Get("sum"), FieldValue::Double(1.5));

  snap = accumulator_.AwaitLocalEvent();
  EXPECT_EQ(snap.Get("sum"), FieldValue::Double(3.5));

  EnableNetwork();

  snap = accumulator_.AwaitRemoteEvent();
  EXPECT_EQ(snap.Get("sum"), FieldValue::Double(3.5));
}

TEST_F(NumericTransformsTest, IncrementTwiceInABatch) {
  WriteInitialData({{"sum", FieldValue::String("overwrite")}});

  WriteBatch batch = TestFirestore()->batch();

  batch.Update(doc_ref_, {{"sum", FieldValue::Increment(1)}});
  batch.Update(doc_ref_, {{"sum", FieldValue::Increment(1)}});

  Await(batch.Commit());

  ExpectLocalAndRemoteValue(2);
}

TEST_F(NumericTransformsTest, IncrementDeleteIncrementInABatch) {
  WriteInitialData({{"sum", FieldValue::String("overwrite")}});

  WriteBatch batch = TestFirestore()->batch();

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
  EXPECT_EQ(snapshot.Get("sum", ServerTimestampBehavior::kEstimate).type(),
            Type::kTimestamp);

  DocumentSnapshot snap = accumulator_.AwaitLocalEvent();
  EXPECT_TRUE(snap.Get("sum").is_integer());
  EXPECT_EQ(snap.Get("sum"), FieldValue::Integer(1));

  EnableNetwork();

  snap = accumulator_.AwaitRemoteEvent();
  EXPECT_TRUE(snap.Get("sum").is_integer());
  EXPECT_EQ(1, snap.Get("sum").integer_value());
}

}  // namespace firestore
}  // namespace firebase
