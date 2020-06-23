#include "firestore/src/tests/firestore_integration_test.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "absl/strings/ascii.h"
#include "Firestore/core/src/util/autoid.h"

namespace firebase {
namespace firestore {

namespace {
// name of FirebaseApp to use for bootstrapping data into Firestore. We use a
// non-default app to avoid data ending up in the cache before tests run.
static const char* kBootstrapAppName = "bootstrap";

void Release(Firestore* firestore) {
  if (firestore == nullptr) {
    return;
  }

  App* app = firestore->app();
  delete firestore;
  delete app;
}

// Set Firestore up to use Firestore Emulator if it can be found.
void LocateEmulator(Firestore* db) {
  // iOS and Android pass emulator address differently, iOS writes it to a
  // temp file, but there is no equivalent to `/tmp/` for Android, so it
  // uses an environment variable instead.
  // TODO(wuandy): See if we can use environment variable for iOS as well?
  std::ifstream ifs("/tmp/emulator_address");
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  std::string address;
  if (ifs.good()) {
    address = buffer.str();
  } else if (std::getenv("FIRESTORE_EMULATOR_HOST")) {
    address = std::getenv("FIRESTORE_EMULATOR_HOST");
  }

  absl::StripAsciiWhitespace(&address);
  if (!address.empty()) {
    auto settings = db->settings();
    settings.set_host(address);
    // Emulator does not support ssl yet.
    settings.set_ssl_enabled(false);
    db->set_settings(settings);
  }
}

}  // anonymous namespace

FirestoreIntegrationTest::FirestoreIntegrationTest() {
  // Allocate the default Firestore eagerly.
  CachedFirestore(kDefaultAppName);
  Firestore::set_log_level(LogLevel::kLogLevelDebug);
}

FirestoreIntegrationTest::~FirestoreIntegrationTest() {
  for (auto named_firestore : firestores_) {
    Await(named_firestore.second->Terminate());
    Release(named_firestore.second);
    firestores_[named_firestore.first] = nullptr;
  }
}

Firestore* FirestoreIntegrationTest::CachedFirestore(
    const std::string& name) const {
  if (firestores_.count(name) > 0) {
    return firestores_[name];
  }

  // Make sure different unit tests don't try to create an app with the same
  // name, because it's not supported by `firebase::App` (the default app is an
  // exception and will be recreated).
  static int counter = 0;
  std::string app_name =
      name == kDefaultAppName ? name : name + std::to_string(counter++);
  Firestore* db = CreateFirestore(app_name);

  firestores_[name] = db;
  return db;
}

Firestore* FirestoreIntegrationTest::CreateFirestore() const {
  static int app_number = 0;
  std::string app_name = "app_for_testing_";
  app_name += std::to_string(app_number++);
  return CreateFirestore(app_name);
}

Firestore* FirestoreIntegrationTest::CreateFirestore(
    const std::string& app_name) const {
  App* app = GetApp(app_name.c_str());
  Firestore* db = new Firestore(CreateTestFirestoreInternal(app));

  LocateEmulator(db);
  InitializeFirestore(db);
  return db;
}

void FirestoreIntegrationTest::DeleteFirestore(const std::string& name) {
  auto found = firestores_.find(name);
  FIREBASE_ASSERT_MESSAGE(
      found != firestores_.end(),
      "Couldn't find Firestore corresponding to app name '%s'", name.c_str());

  TerminateAndRelease(found->second);
  firestores_.erase(found);
}

CollectionReference FirestoreIntegrationTest::Collection() const {
  return firestore()->Collection(util::CreateAutoId());
}

CollectionReference FirestoreIntegrationTest::Collection(
    const std::string& name_prefix) const {
  return firestore()->Collection(name_prefix + "_" + util::CreateAutoId());
}

CollectionReference FirestoreIntegrationTest::Collection(
    const std::map<std::string, MapFieldValue>& docs) const {
  CollectionReference result = Collection();
  WriteDocuments(CachedFirestore(kBootstrapAppName)->Collection(result.path()),
                 docs);
  return result;
}

std::string FirestoreIntegrationTest::DocumentPath() const {
  return "test-collection/" + util::CreateAutoId();
}

DocumentReference FirestoreIntegrationTest::Document() const {
  return firestore()->Document(DocumentPath());
}

void FirestoreIntegrationTest::WriteDocument(DocumentReference reference,
                                             const MapFieldValue& data) const {
  Future<void> future = reference.Set(data);
  Await(future);
  EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
  EXPECT_EQ(0, future.error()) << DescribeFailedFuture(future) << std::endl;
}

void FirestoreIntegrationTest::WriteDocuments(
    CollectionReference reference,
    const std::map<std::string, MapFieldValue>& data) const {
  for (const auto& kv : data) {
    WriteDocument(reference.Document(kv.first), kv.second);
  }
}

DocumentSnapshot FirestoreIntegrationTest::ReadDocument(
    const DocumentReference& reference) const {
  Future<DocumentSnapshot> future = reference.Get();
  const DocumentSnapshot* result = Await(future);
  EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
  EXPECT_EQ(0, future.error()) << DescribeFailedFuture(future) << std::endl;
  EXPECT_NE(nullptr, result) << DescribeFailedFuture(future) << std::endl;
  return *result;
}

QuerySnapshot FirestoreIntegrationTest::ReadDocuments(
    const Query& reference) const {
  Future<QuerySnapshot> future = reference.Get();
  const QuerySnapshot* result = Await(future);
  EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
  EXPECT_EQ(0, future.error()) << DescribeFailedFuture(future) << std::endl;
  EXPECT_NE(nullptr, result) << DescribeFailedFuture(future) << std::endl;
  return *result;
}

void FirestoreIntegrationTest::DeleteDocument(
    DocumentReference reference) const {
  Future<void> future = reference.Delete();
  Await(future);
  EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
  EXPECT_EQ(0, future.error()) << DescribeFailedFuture(future) << std::endl;
}

std::vector<std::string> FirestoreIntegrationTest::QuerySnapshotToIds(
    const QuerySnapshot& snapshot) const {
  std::vector<std::string> result;
  for (const DocumentSnapshot& doc : snapshot.documents()) {
    result.push_back(doc.id());
  }
  return result;
}

std::vector<MapFieldValue> FirestoreIntegrationTest::QuerySnapshotToValues(
    const QuerySnapshot& snapshot) const {
  std::vector<MapFieldValue> result;
  for (const DocumentSnapshot& doc : snapshot.documents()) {
    result.push_back(doc.GetData());
  }
  return result;
}

/* static */
void FirestoreIntegrationTest::Await(const Future<void>& future) {
  while (future.status() == FutureStatus::kFutureStatusPending) {
    if (ProcessEvents(kCheckIntervalMillis)) {
      std::cout << "WARNING: app received an event requesting exit."
                << std::endl;
      break;
    }
  }
}

void FirestoreIntegrationTest::TerminateAndRelease(Firestore* firestore) {
  Await(firestore->Terminate());
  Release(firestore);
}

}  // namespace firestore
}  // namespace firebase
