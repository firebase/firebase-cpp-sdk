#include <string>

#include "devtools/build/runtime/get_runfiles_dir.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
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

// This test makes sure that all the objects in Firestore public API are
// available from just including "firestore.h".
// If this test compiles, that is sufficient.
// Not using `FirestoreIntegrationTest` to avoid any headers it includes.
TEST_F(IncludesTest, TestIncludingFirestoreHeaderIsSufficient) {
  std::string google_json_dir = devtools_build::testonly::GetTestSrcdir() +
                                "/google3/firebase/firestore/client/cpp/";
  App::SetDefaultConfigPath(google_json_dir.c_str());

#if defined(__ANDROID__)
  App* app = App::Create(nullptr, nullptr);

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

}  // namespace
}  // namespace firestore
}  // namespace firebase
