#include <utility>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#if defined(__ANDROID__)
#include "firestore/src/android/listener_registration_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/listener_registration_stub.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRListenerRegistrationTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/ListenerRegistrationTest.java

namespace firebase {
namespace firestore {

using ListenerRegistrationCommonTest = testing::Test;

class ListenerRegistrationTest : public FirestoreIntegrationTest {
 public:
  ListenerRegistrationTest() {
    TestFirestore()->set_log_level(LogLevel::kLogLevelDebug);
  }
};

// These tests don't work with stubs.
#if !defined(FIRESTORE_STUB_BUILD)

TEST_F(ListenerRegistrationTest, TestCanBeRemoved) {
  CollectionReference collection = Collection();
  DocumentReference document = collection.Document();

  TestEventListener<QuerySnapshot> listener_one("a listener to be removed");
  TestEventListener<DocumentSnapshot> listener_two("a listener to be removed");
  ListenerRegistration one = listener_one.AttachTo(&collection);
  ListenerRegistration two = listener_two.AttachTo(&document);

  // Initial events
  Await(listener_one);
  Await(listener_two);
  EXPECT_EQ(1, listener_one.event_count());
  EXPECT_EQ(1, listener_two.event_count());

  // Trigger new events
  WriteDocument(document, {{"foo", FieldValue::String("bar")}});

  // Write events should have triggered
  EXPECT_EQ(2, listener_one.event_count());
  EXPECT_EQ(2, listener_two.event_count());

  // No more events should occur
  one.Remove();
  two.Remove();

  WriteDocument(document, {{"foo", FieldValue::String("new-bar")}});

  // Assert no events actually occurred
  EXPECT_EQ(2, listener_one.event_count());
  EXPECT_EQ(2, listener_two.event_count());
}

TEST_F(ListenerRegistrationTest, TestCanBeRemovedTwice) {
  CollectionReference collection = Collection();
  DocumentReference document = collection.Document();

  TestEventListener<QuerySnapshot> listener_one("a listener to be removed");
  TestEventListener<DocumentSnapshot> listener_two("a listener to be removed");
  ListenerRegistration one = listener_one.AttachTo(&collection);
  ListenerRegistration two = listener_two.AttachTo(&document);

  one.Remove();
  EXPECT_NO_THROW(one.Remove());

  two.Remove();
  EXPECT_NO_THROW(two.Remove());
}

TEST_F(ListenerRegistrationTest, TestCanBeRemovedIndependently) {
  CollectionReference collection = Collection();
  DocumentReference document = collection.Document();

  TestEventListener<QuerySnapshot> listener_one("listener one");
  TestEventListener<QuerySnapshot> listener_two("listener two");
  ListenerRegistration one = listener_one.AttachTo(&collection);
  ListenerRegistration two = listener_two.AttachTo(&collection);

  // Initial events
  Await(listener_one);
  Await(listener_two);

  // Triger new events
  WriteDocument(document, {{"foo", FieldValue::String("bar")}});

  // Write events should have triggered
  EXPECT_EQ(2, listener_one.event_count());
  EXPECT_EQ(2, listener_two.event_count());

  // Should leave listener number two unaffected
  one.Remove();

  WriteDocument(document, {{"foo", FieldValue::String("new-bar")}});

  // Assert only events for listener number two actually occurred
  EXPECT_EQ(2, listener_one.event_count());
  EXPECT_EQ(3, listener_two.event_count());

  // No more events should occur
  two.Remove();

  // The following check does not exist in the corresponding Android and iOS
  // native client SDKs tests.
  WriteDocument(document, {{"foo", FieldValue::String("brand-new-bar")}});
  EXPECT_EQ(2, listener_one.event_count());
  EXPECT_EQ(3, listener_two.event_count());
}

#endif  // defined(FIRESTORE_STUB_BUILD)

#if defined(__ANDROID__)
// TODO(b/136011600): the mechanism for creating internals doesn't work on iOS.
// The most valuable test is making sure that a copy of a registration can be
// used to remove the listener.

TEST_F(ListenerRegistrationCommonTest, Construction) {
  auto* internal = testutil::NewInternal<ListenerRegistrationInternal>();
  auto registration = MakePublic<ListenerRegistration>(internal);
  EXPECT_EQ(internal, GetInternal(registration));

  ListenerRegistration reg_default;
  EXPECT_EQ(nullptr, GetInternal(reg_default));

  ListenerRegistration reg_copy(registration);
  EXPECT_EQ(internal, GetInternal(reg_copy));

  ListenerRegistration reg_move(std::move(registration));
  EXPECT_EQ(internal, GetInternal(reg_move));

  // ListenerRegistrations are normally owned by FirestoreInternal so the
  // public ListenerRegistration does not delete the internal instance.
  delete internal;
}

TEST_F(ListenerRegistrationCommonTest, Assignment) {
  auto* internal = testutil::NewInternal<ListenerRegistrationInternal>();
  auto registration = MakePublic<ListenerRegistration>(internal);
  ListenerRegistration reg_copy;
  reg_copy = registration;
  EXPECT_EQ(internal, GetInternal(reg_copy));

  ListenerRegistration reg_move;
  reg_move = std::move(registration);
  EXPECT_EQ(internal, GetInternal(reg_move));

  // ListenerRegistrations are normally owned by FirestoreInternal so the
  // public ListenerRegistration does not delete the internal instance.
  delete internal;
}

TEST_F(ListenerRegistrationCommonTest, Remove) {
  auto* internal = testutil::NewInternal<ListenerRegistrationInternal>();
  auto registration = MakePublic<ListenerRegistration>(internal);
  ListenerRegistration reg_copy;
  reg_copy = registration;

  registration.Remove();
  reg_copy.Remove();

  // ListenerRegistrations are normally owned by FirestoreInternal so the
  // public ListenerRegistration does not delete the internal instance.
  delete internal;
}

#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
