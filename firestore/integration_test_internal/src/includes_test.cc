#include <string>

#include "firebase/firestore.h"
#include "firestore_integration_test.h"
#include "app_framework.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

// This class is a friend of `Firestore`, necessary to access `GetTestInstance`.
class IncludesTest : public testing::Test {
 public:
  Firestore* CreateFirestore(App* app) {
    return new Firestore(CreateTestFirestoreInternal(app));
  }
};

namespace {

struct TestListener : EventListener<int> {
  void OnEvent(const int&, Error, const std::string&) override {}
};

struct TestTransactionFunction : TransactionFunction {
  Error Apply(Transaction&, std::string&) override { return Error::kErrorOk; }
};

}  // namespace

// This test makes sure that all the objects in Firestore public API are
// available from just including "firestore.h".
// If this test compiles, that is sufficient.
// Not using `FirestoreIntegrationTest` to avoid any headers it includes.
TEST_F(IncludesTest, TestIncludingFirestoreHeaderIsSufficient) {
  // We don't actually need to run any of the below, just compile it.
  if (0) {
#if defined(__ANDROID__)
    App* app = App::Create(app_framework::GetJniEnv(), app_framework::GetActivity());

#elif defined(FIRESTORE_STUB_BUILD)
    // Stubs don't load values from `GoogleService-Info.plist`/etc., so the app
    // has to be configured explicitly.
    AppOptions options;
    options.set_project_id("foo");
    options.set_app_id("foo");
    options.set_api_key("foo");
    App* app = App::Create(options);

#else
    App* app = App::Create();
#endif  // defined(__ANDROID__)

    Firestore* firestore = CreateFirestore(app);

    {
      // Check that Firestore isn't just forward-declared.
      DocumentReference doc = firestore->Document("foo/bar");
      Future<DocumentSnapshot> future = doc.Get();
      DocumentChange doc_change;
      DocumentReference doc_ref;
      DocumentSnapshot doc_snap;
      FieldPath field_path;
      FieldValue field_value;
      ListenerRegistration listener_registration;
      MapFieldValue map_field_value;
      MetadataChanges metadata_changes = MetadataChanges::kExclude;
      Query query;
      QuerySnapshot query_snapshot;
      SetOptions set_options;
      Settings settings;
      SnapshotMetadata snapshot_metadata;
      Source source = Source::kDefault;
      // Cannot default-construct a `Transaction`.
      WriteBatch write_batch;

      TestListener test_listener;
      TestTransactionFunction test_transaction_function;

      Timestamp timestamp;
      GeoPoint geo_point;
      Error error = Error::kErrorOk;
    }
    delete firestore;
    delete app;
  }
}

}  // namespace firestore
}  // namespace firebase
