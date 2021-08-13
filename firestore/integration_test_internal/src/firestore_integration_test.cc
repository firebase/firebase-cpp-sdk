/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore_integration_test.h"

#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

#if !defined(__ANDROID__)
#include "Firestore/core/src/util/autoid.h"
#include "absl/strings/ascii.h"
#else
#include "android/util_autoid.h"
#endif  // !defined(__ANDROID__)
#include "app_framework.h"

namespace firebase {
namespace firestore {

namespace {
// name of FirebaseApp to use for bootstrapping data into Firestore. We use a
// non-default app to avoid data ending up in the cache before tests run.
static const char* kBootstrapAppName = "bootstrap";

// Set Firestore up to use Firestore Emulator via USE_FIRESTORE_EMULATOR
void LocateEmulator(Firestore* db) {
  // Use emulator as long as this env variable is set, regardless its value.
  if (std::getenv("USE_FIRESTORE_EMULATOR") == nullptr) {
    LogDebug("Using Firestore Prod for testing.");
    return;
  }

#if defined(__ANDROID__)
  // Special IP to access the hosting OS from Android Emulator.
  std::string local_host = "10.0.2.2";
#else
  std::string local_host = "localhost";
#endif  // defined(__ANDROID__)

  // Use FIRESTORE_EMULATOR_PORT if it is set to non empty string,
  // otherwise use the default port.
  std::string port = std::getenv("FIRESTORE_EMULATOR_PORT")
                         ? std::getenv("FIRESTORE_EMULATOR_PORT")
                         : "8080";
  std::string address =
      port.empty() ? (local_host + ":8080") : (local_host + ":" + port);

  LogInfo("Using Firestore Emulator (%s) for testing.", address.c_str());
  auto settings = db->settings();
  settings.set_host(address);
  // Emulator does not support ssl yet.
  settings.set_ssl_enabled(false);
  db->set_settings(settings);
}

}  // namespace

std::string ToFirestoreErrorCodeName(int error_code) {
  switch (error_code) {
    case kErrorOk:
      return "kErrorOk";
    case kErrorCancelled:
      return "kErrorCancelled";
    case kErrorUnknown:
      return "kErrorUnknown";
    case kErrorInvalidArgument:
      return "kErrorInvalidArgument";
    case kErrorDeadlineExceeded:
      return "kErrorDeadlineExceeded";
    case kErrorNotFound:
      return "kErrorNotFound";
    case kErrorAlreadyExists:
      return "kErrorAlreadyExists";
    case kErrorPermissionDenied:
      return "kErrorPermissionDenied";
    case kErrorResourceExhausted:
      return "kErrorResourceExhausted";
    case kErrorFailedPrecondition:
      return "kErrorFailedPrecondition";
    case kErrorAborted:
      return "kErrorAborted";
    case kErrorOutOfRange:
      return "kErrorOutOfRange";
    case kErrorUnimplemented:
      return "kErrorUnimplemented";
    case kErrorInternal:
      return "kErrorInternal";
    case kErrorUnavailable:
      return "kErrorUnavailable";
    case kErrorDataLoss:
      return "kErrorDataLoss";
    case kErrorUnauthenticated:
      return "kErrorUnauthenticated";
    default:
      return "[invalid error code]";
  }
}

int WaitFor(const FutureBase& future) {
  // Instead of getting a clock, we count the cycles instead.
  int cycles = kTimeOutMillis / kCheckIntervalMillis;
  while (future.status() == FutureStatus::kFutureStatusPending && cycles > 0) {
    if (ProcessEvents(kCheckIntervalMillis)) {
      std::cout << "WARNING: app receives an event requesting exit."
                << std::endl;
      break;
    }
    --cycles;
  }
  return cycles;
}

FirestoreIntegrationTest::FirestoreIntegrationTest() {
  // Allocate the default Firestore eagerly.
  TestFirestore();
}

Firestore* FirestoreIntegrationTest::TestFirestore(
    const std::string& name) const {
  return TestFirestoreWithProjectId(name, /*project_id=*/"");
}

Firestore* FirestoreIntegrationTest::TestFirestoreWithProjectId(
    const std::string& name, const std::string& project_id) const {
  for (const auto& entry : firestores_) {
    const FirestoreInfo& firestore_info = entry.second;
    if (firestore_info.name() == name) {
      return firestore_info.firestore();
    }
  }

  App* app = GetApp(name.c_str(), project_id);
  if (apps_.find(app) == apps_.end()) {
    apps_[app] = std::unique_ptr<App>(app);
  }

  // Firestore::set_log_level(LogLevel::kLogLevelDebug);

  Firestore* db = new Firestore(CreateTestFirestoreInternal(app));
  firestores_[db] = FirestoreInfo(name, std::unique_ptr<Firestore>(db));

  LocateEmulator(db);
  InitializeFirestore(db);
  return db;
}

void FirestoreIntegrationTest::DeleteFirestore(Firestore* firestore) {
  auto found = firestores_.find(firestore);
  FIREBASE_ASSERT_MESSAGE(found != firestores_.end(),
                          "The given Firestore was not found.");
  firestores_.erase(found);
}

void FirestoreIntegrationTest::DisownFirestore(Firestore* firestore) {
  auto found = firestores_.find(firestore);
  FIREBASE_ASSERT_MESSAGE(found != firestores_.end(),
                          "The given Firestore was not found.");
  found->second.ReleaseFirestore();
  firestores_.erase(found);
}

void FirestoreIntegrationTest::DeleteApp(App* app) {
  auto found = apps_.find(app);
  FIREBASE_ASSERT_MESSAGE(found != apps_.end(), "The given App was not found.");

  // Remove the Firestore instances from our internal list that are owned by the
  // given App. Deleting the App also deletes the Firestore instances created
  // via that App; therefore, removing our references to those Firestore
  // instances avoids double-deletion and also avoids returning deleted
  // Firestore instances from TestFirestore().
  auto firestores_iterator = firestores_.begin();
  while (firestores_iterator != firestores_.end()) {
    if (firestores_iterator->first->app() == app) {
      firestores_iterator = firestores_.erase(firestores_iterator);
    } else {
      ++firestores_iterator;
    }
  }

  apps_.erase(found);
}

CollectionReference FirestoreIntegrationTest::Collection() const {
  return TestFirestore()->Collection(util::CreateAutoId());
}

CollectionReference FirestoreIntegrationTest::Collection(
    const std::string& name_prefix) const {
  return TestFirestore()->Collection(name_prefix + "_" + util::CreateAutoId());
}

CollectionReference FirestoreIntegrationTest::Collection(
    const std::map<std::string, MapFieldValue>& docs) const {
  CollectionReference result = Collection();
  WriteDocuments(TestFirestore(kBootstrapAppName)->Collection(result.path()),
                 docs);
  return result;
}

std::string FirestoreIntegrationTest::DocumentPath() const {
  return "test-collection/" + util::CreateAutoId();
}

DocumentReference FirestoreIntegrationTest::Document() const {
  return TestFirestore()->Document(DocumentPath());
}

DocumentReference FirestoreIntegrationTest::DocumentWithData(
    const MapFieldValue& data) const {
  DocumentReference docRef = Document();
  WriteDocument(docRef, data);
  return docRef;
}

void FirestoreIntegrationTest::WriteDocument(DocumentReference reference,
                                             const MapFieldValue& data) const {
  Future<void> future = reference.Set(data);
  Await(future);
  FailIfUnsuccessful("WriteDocument", future);
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
  if (FailIfUnsuccessful("ReadDocument", future)) {
    return {};
  } else {
    return *result;
  }
}

QuerySnapshot FirestoreIntegrationTest::ReadDocuments(
    const Query& reference) const {
  Future<QuerySnapshot> future = reference.Get();
  const QuerySnapshot* result = Await(future);
  if (FailIfUnsuccessful("ReadDocuments", future)) {
    return {};
  } else {
    return *result;
  }
}

void FirestoreIntegrationTest::DeleteDocument(
    DocumentReference reference) const {
  Future<void> future = reference.Delete();
  Await(future);
  FailIfUnsuccessful("DeleteDocument", future);
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

std::map<std::string, MapFieldValue>
FirestoreIntegrationTest::QuerySnapshotToMap(
    const QuerySnapshot& snapshot) const {
  std::map<std::string, MapFieldValue> result;
  for (const DocumentSnapshot& doc : snapshot.documents()) {
    result[doc.id()] = doc.GetData();
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

/* static */
bool FirestoreIntegrationTest::FailIfUnsuccessful(const char* operation,
                                                  const FutureBase& future) {
  if (future.status() != FutureStatus::kFutureStatusComplete) {
    ADD_FAILURE() << operation << " timed out: " << DescribeFailedFuture(future)
                  << std::endl;
    return true;
  } else if (future.error() != Error::kErrorOk) {
    ADD_FAILURE() << operation << " failed: " << DescribeFailedFuture(future)
                  << std::endl;
    return true;
  } else {
    return false;
  }
}

/* static */
std::string FirestoreIntegrationTest::DescribeFailedFuture(
    const FutureBase& future) {
  return "Future failed: " + ToFirestoreErrorCodeName(future.error()) + " (" +
         std::to_string(future.error()) + "): " + future.error_message();
}

bool ProcessEvents(int msec) { return app_framework::ProcessEvents(msec); }

}  // namespace firestore
}  // namespace firebase
