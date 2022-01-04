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

#include <string>

#include "app_framework.h"
#include "firebase/firestore.h"
#include "firestore_integration_test.h"
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

struct TestListener {
  void OnEvent(const int&, Error, const std::string&) {}
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
    App* app =
        App::Create(app_framework::GetJniEnv(), app_framework::GetActivity());
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
