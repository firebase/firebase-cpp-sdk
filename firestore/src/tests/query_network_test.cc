#include <cmath>
#include <utility>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#if defined(__ANDROID__)
#include "firestore/src/android/query_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/query_stub.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRQueryTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/QueryTest.java

namespace firebase {
namespace firestore {

class QueryNetworkTest : public FirestoreIntegrationTest {
 protected:
  void TestCanHaveMultipleMutationsWhileOfflineImpl() {
    CollectionReference collection = Collection();

    // set a few docs to known values
    WriteDocuments(collection,
                   {{"doc1", {{"key1", FieldValue::String("value1")}}},
                    {"doc2", {{"key2", FieldValue::String("value2")}}}});

    // go offline for the rest of this test
    Await(TestFirestore()->DisableNetwork());

    // apply *multiple* mutations while offline
    collection.Document("doc1").Set({{"key1b", FieldValue::String("value1b")}});
    collection.Document("doc2").Set({{"key2b", FieldValue::String("value2b")}});

    QuerySnapshot snapshot = ReadDocuments(collection);
    EXPECT_TRUE(snapshot.metadata().is_from_cache());
    EXPECT_THAT(QuerySnapshotToValues(snapshot),
                testing::ElementsAre(
                    MapFieldValue{{"key1b", FieldValue::String("value1b")}},
                    MapFieldValue{{"key2b", FieldValue::String("value2b")}}));

    Await(TestFirestore()->EnableNetwork());
  }

  void TestWatchSurvivesNetworkDisconnectImpl() {
    CollectionReference collection = Collection();
    EventAccumulator<QuerySnapshot> accumulator;
    accumulator.listener()->set_print_debug_info(true);
    ListenerRegistration registration = accumulator.listener()->AttachTo(
        &collection, MetadataChanges::kInclude);
    EXPECT_TRUE(accumulator.AwaitRemoteEvent().empty());

    Await(TestFirestore()->DisableNetwork());
    auto added =
        collection.Add(MapFieldValue{{"foo", FieldValue::ServerTimestamp()}});
    Await(TestFirestore()->EnableNetwork());
    Await(added);

    QuerySnapshot snapshot = accumulator.AwaitServerEvent();
    EXPECT_FALSE(snapshot.empty());
    EXPECT_EQ(1, snapshot.size());

    registration.Remove();
  }

  void TestQueriesFireFromCacheWhenOfflineImpl() {
    CollectionReference collection =
        Collection({{"a", {{"foo", FieldValue::Integer(1)}}}});
    EventAccumulator<QuerySnapshot> accumulator;
    accumulator.listener()->set_print_debug_info(true);
    ListenerRegistration registration = accumulator.listener()->AttachTo(
        &collection, MetadataChanges::kInclude);

    // initial event
    QuerySnapshot snapshot = accumulator.AwaitServerEvent();
    EXPECT_THAT(
        QuerySnapshotToValues(snapshot),
        testing::ElementsAre(MapFieldValue{{"foo", FieldValue::Integer(1)}}));
    EXPECT_FALSE(snapshot.metadata().is_from_cache());

    // offline event with is_from_cache=true
    Await(TestFirestore()->DisableNetwork());
    snapshot = accumulator.Await();
    EXPECT_TRUE(snapshot.metadata().is_from_cache());

    // back online event with is_from_cache=false
    Await(TestFirestore()->EnableNetwork());
    snapshot = accumulator.Await();
    EXPECT_FALSE(snapshot.metadata().is_from_cache());
    registration.Remove();
  }
};

#if defined(__ANDROID__)
// Due to how the integration test is set on Android, we cannot make the tests
// that call DisableNetwork/EnableNetwork run in parallel. So we manually make
// them here in a separate test file and run in serial.

TEST_F(QueryNetworkTest, EnableDisableNetwork) {
  std::cout
      << "[ RUN      ] "
         "FirestoreIntegrationTest.TestCanHaveMultipleMutationsWhileOffline"
      << std::endl;
  TestCanHaveMultipleMutationsWhileOfflineImpl();
  std::cout
      << "[     DONE ] "
         "FirestoreIntegrationTest.TestCanHaveMultipleMutationsWhileOffline"
      << std::endl;

  std::cout
      << "[ RUN      ] FirestoreIntegrationTest.WatchSurvivesNetworkDisconnect"
      << std::endl;
  TestWatchSurvivesNetworkDisconnectImpl();
  std::cout
      << "[     DONE ] FirestoreIntegrationTest.WatchSurvivesNetworkDisconnect"
      << std::endl;

  std::cout << "[ RUN      ] "
               "FirestoreIntegrationTest.TestQueriesFireFromCacheWhenOffline"
            << std::endl;
  TestQueriesFireFromCacheWhenOfflineImpl();
  std::cout << "[     DONE ] "
               "FirestoreIntegrationTest.TestQueriesFireFromCacheWhenOffline"
            << std::endl;
}

#else

TEST_F(QueryNetworkTest, TestCanHaveMultipleMutationsWhileOffline) {
  TestCanHaveMultipleMutationsWhileOfflineImpl();
}

TEST_F(QueryNetworkTest, TestWatchSurvivesNetworkDisconnect) {
  TestWatchSurvivesNetworkDisconnectImpl();
}

TEST_F(QueryNetworkTest, TestQueriesFireFromCacheWhenOffline) {
  TestQueriesFireFromCacheWhenOfflineImpl();
}

#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
