// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "app/rest/util.h"
#include "app/src/app_state.h"
#include "app/src/include/firebase/app.h"
#include "app/src/service_interfaces.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "storage/src/common/list_result_internal.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/metadata_desktop.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "testing/json_util.h"

namespace {

using firebase::App;
using firebase::storage::internal::MetadataInternal;
using firebase::storage::internal::StorageInternal;
using firebase::storage::internal::StoragePath;
using firebase::storage::internal::StorageReferenceInternal;

// The fixture for testing helper classes for storage desktop.
class StorageDesktopUtilsTests : public ::testing::Test {
 protected:
  void SetUp() override { firebase::rest::util::Initialize(); }

  void TearDown() override { firebase::rest::util::Terminate(); }
};

// Test the GS URI-based StoragePath constructors
TEST_F(StorageDesktopUtilsTests, testGSStoragePathConstructors) {
  StoragePath test_path;

  // Test basic case:
  test_path = StoragePath(nullptr, "gs://Bucket/path/Object");

  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/Object");

  // Test a more complex path:
  test_path = StoragePath(nullptr, "gs://Bucket/path/morepath/Object");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/morepath/Object");

  // Extra slashes:
  test_path = StoragePath(nullptr, "gs://Bucket/path////Object");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/Object");

  // Path with no Object:
  test_path = StoragePath(nullptr, "gs://Bucket/path////more////");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/more");
}

// Test the HTTP(S)-based StoragePath constructors
TEST_F(StorageDesktopUtilsTests, testHTTPStoragePathConstructors) {
  StoragePath test_path;
  std::string intended_bucket_result = "Bucket";
  std::string intended_path_result = "path/to/Object/Object.data";

  // Test basic case:
  test_path = StoragePath(nullptr,
                          "http://firebasestorage.googleapis.com/v0/b/Bucket/o/"
                          "path%2fto%2FObject%2fObject.data");
  EXPECT_STREQ(test_path.GetBucket().c_str(), intended_bucket_result.c_str());
  EXPECT_STREQ(test_path.GetPath().c_str(), intended_path_result.c_str());

  // httpS (instead of http):
  test_path =
      StoragePath(nullptr,
                  "https://firebasestorage.googleapis.com/v0/b/Bucket/o/"
                  "path%2fto%2FObject%2fObject.data");
  EXPECT_STREQ(test_path.GetBucket().c_str(), intended_bucket_result.c_str());
  EXPECT_STREQ(test_path.GetPath().c_str(), intended_path_result.c_str());

  // Extra slashes:
  test_path = StoragePath(nullptr,
                          "http://firebasestorage.googleapis.com/v0/b/Bucket/o/"
                          "path%2f%2f%2f%2fto%2FObject%2f%2f%2f%2fObject.data");
  EXPECT_STREQ(test_path.GetBucket().c_str(), intended_bucket_result.c_str());
  EXPECT_STREQ(test_path.GetPath().c_str(), intended_path_result.c_str());
}

TEST_F(StorageDesktopUtilsTests, testInvalidConstructors) {
  StoragePath bad_path(nullptr, "argleblargle://Bucket/path1/path2/Object");
  EXPECT_FALSE(bad_path.IsValid());
}

// Test the StoragePath.Parent() function.
TEST_F(StorageDesktopUtilsTests, testStoragePathParent) {
  StoragePath test_path;

  // Test parent, when there is an GetObject.
  test_path = StoragePath(nullptr, "gs://Bucket/path/Object").GetParent();
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path");

  // Test parent with no GetObject.
  test_path = StoragePath(nullptr, "gs://Bucket/path/morepath/").GetParent();
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path");
}

// Test the StoragePath.Child() function.
TEST_F(StorageDesktopUtilsTests, testStoragePathChild) {
  StoragePath test_path;

  // Test child when there is no object.
  test_path =
      StoragePath(nullptr, "gs://Bucket/path/morepath/").GetChild("newobj");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/morepath/newobj");

  // Test child when there is an object.
  test_path =
      StoragePath(nullptr, "gs://Bucket/path/object").GetChild("newpath/");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/object/newpath");
}

TEST_F(StorageDesktopUtilsTests, testUrlConverter) {
  App app = firebase::testing::CreateApp();
  auto app_state = firebase::internal::AppState::FromApp(app);
  StorageInternal storage(*app_state, app, "gs://Bucket");

  StoragePath test_path(&storage, "gs://Bucket/path1/path2/Object");

  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path1/path2/Object");

  EXPECT_STREQ(test_path.AsHttpUrl().c_str(),
               "https://firebasestorage.googleapis.com:443"
               "/v0/b/Bucket/o/path1%2Fpath2%2FObject?alt=media");
  EXPECT_STREQ(test_path.AsHttpMetadataUrl().c_str(),
               "https://firebasestorage.googleapis.com:443"
               "/v0/b/Bucket/o/path1%2Fpath2%2FObject");
}

TEST_F(StorageDesktopUtilsTests, testMetadataJsonExporter) {
  App app = firebase::testing::CreateApp();
  firebase::storage::Storage storage =
      firebase::storage::Storage::GetInstance(app, "gs://abucket");
  ASSERT_TRUE(storage.is_valid());
  firebase::storage::StorageReference reference =
      storage.GetReferenceFromUrl("gs://abucket/path/to/a/file.txt");
  ASSERT_TRUE(reference.is_valid());
  MetadataInternal metadata(reference);
  reference = firebase::storage::StorageReference();

  metadata.set_cache_control("cache_control_test");
  metadata.set_content_disposition("content_disposition_test");
  metadata.set_content_encoding("content_encoding_test");
  metadata.set_content_language("content_language_test");
  metadata.set_content_type("content_type_test");

  std::map<std::string, std::string>& custom_metadata =
      *metadata.custom_metadata();
  custom_metadata["key1"] = "value1";
  custom_metadata["key2"] = "value2";
  custom_metadata["key3"] = "value3";

  std::string json = metadata.ExportAsJson();

  // clang-format=off
  EXPECT_THAT(json, ::firebase::testing::cppsdk::EqualsJson(
                        "{\"bucket\":\"abucket\","
                        "\"cacheControl\":\"cache_control_test\","
                        "\"contentDisposition\":\"content_disposition_test\","
                        "\"contentEncoding\":\"content_encoding_test\","
                        "\"contentLanguage\":\"content_language_test\","
                        "\"contentType\":\"content_type_test\","
                        "\"metadata\":"
                        "{\"key1\":\"value1\","
                        "\"key2\":\"value2\","
                        "\"key3\":\"value3\"},"
                        "\"name\":\"file.txt\"}"));
  // clang-format=on
}

TEST_F(StorageDesktopUtilsTests, LifecycleKeepaliveAndWeakMetadata) {
  firebase::storage::StorageReference ref;
  std::weak_ptr<firebase::internal::AppState> weak_state;
  {
    App app = firebase::testing::CreateApp(firebase::testing::MockAppOptions(),
                                           "storage_keepalive_app");
    weak_state = firebase::internal::AppState::FromApp(app);
    firebase::storage::Storage storage =
        firebase::storage::Storage::GetInstance(app, "gs://my-bucket");
    ASSERT_TRUE(storage.is_valid());
    EXPECT_EQ(storage,
              firebase::storage::Storage::GetInstance(app, "gs://my-bucket"));
    ref = storage.GetReference("dir/file.txt");
    ASSERT_TRUE(ref.is_valid());
  }
  EXPECT_FALSE(weak_state.expired());
  EXPECT_TRUE(ref.is_valid());
  EXPECT_TRUE(ref.storage().is_valid());
  EXPECT_STREQ("storage_keepalive_app", ref.storage().app().name());

  // Metadata holds weak_ptr<StorageInternal>: no cycle when ref drops.
  MetadataInternal meta(ref);
  {
    std::unique_ptr<StorageReferenceInternal> meta_ref(meta.GetReference());
    EXPECT_TRUE(meta_ref != nullptr);
  }
  ref = firebase::storage::StorageReference();
  EXPECT_TRUE(weak_state.expired());
  EXPECT_EQ(nullptr, meta.GetReference());
}

TEST_F(StorageDesktopUtilsTests, StaticDurationStorageSurvivesExit) {
  static firebase::storage::Storage static_storage = []() {
    App app = firebase::testing::CreateApp(firebase::testing::MockAppOptions(),
                                           "storage_static_exit");
    return firebase::storage::Storage::GetInstance(app, "gs://static-bucket");
  }();
  EXPECT_TRUE(static_storage.is_valid());
}

TEST_F(StorageDesktopUtilsTests,
       StorageListResultWeakStorageButCopiedReferenceIsStrong) {
  std::weak_ptr<firebase::internal::AppState> weak_state;
  firebase::storage::StorageListResult list_result;
  firebase::storage::StorageReference strong_item_copy;
  {
    App app = firebase::testing::CreateApp(firebase::testing::MockAppOptions(),
                                           "storage_list_result_app");
    weak_state = firebase::internal::AppState::FromApp(app);
    firebase::storage::Storage storage =
        firebase::storage::Storage::GetInstance(app, "gs://list-bucket");
    ASSERT_TRUE(storage.is_valid());
    firebase::storage::StorageReference item_ref =
        storage.GetReference("folder/item.txt");
    ASSERT_TRUE(item_ref.is_valid());

    firebase::storage::StorageReference weak_ref =
        StorageReferenceInternal::CloneWeakReference(item_ref);
    std::vector<firebase::storage::StorageReference> items;
    items.push_back(std::move(weak_ref));
    auto* internal_list =
        new firebase::storage::internal::StorageListResultInternal(
            {}, std::move(items), "next_tok");
    list_result = firebase::storage::internal::StorageListResultInternal::
        AsStorageListResult(internal_list);

    // Copying StorageListResult (as FutureManager::CompleteWithResult does)
    // must keep internal references weak so no reference cycle can form.
    firebase::storage::StorageListResult copied_list(list_result);
    ASSERT_EQ(1u, copied_list.items().size());
    EXPECT_EQ("next_tok", copied_list.next_page_token());
    // Copying a StorageReference out of StorageListResult promotes it to
    // strong so user handles keep AppState alive.
    strong_item_copy = copied_list.items()[0];
    EXPECT_TRUE(strong_item_copy.is_valid());
  }

  EXPECT_FALSE(weak_state.expired());
  EXPECT_TRUE(strong_item_copy.is_valid());
  EXPECT_TRUE(strong_item_copy.storage().is_valid());

  // Dropping strong_item_copy must let AppState tear down even while
  // list_result is still in scope.
  strong_item_copy = firebase::storage::StorageReference();
  EXPECT_TRUE(weak_state.expired());
  ASSERT_EQ(1u, list_result.items().size());
  EXPECT_FALSE(list_result.items()[0].is_valid());
}

TEST_F(StorageDesktopUtilsTests,
       StorageCreatedBeforeAuthActivatesTokenRefresh) {
  struct FakeAuthProvider : public firebase::internal::AuthTokenProvider {
    std::string GetCurrentToken() override { return "tok"; }
    firebase::Future<std::string> GetTokenAsync(bool) override { return {}; }
    std::string GetCurrentUserUid() override { return "uid"; }
    void StartTokenListener() override { start_count.fetch_add(1); }
    void StopTokenListener() override { stop_count.fetch_add(1); }

    std::atomic<int> start_count{0};
    std::atomic<int> stop_count{0};
  };

  std::weak_ptr<firebase::internal::AppState> weak_state;
  std::shared_ptr<FakeAuthProvider> raw_provider;
  {
    App app = firebase::testing::CreateApp(firebase::testing::MockAppOptions(),
                                           "storage_before_auth_app");
    auto state = firebase::internal::AppState::FromApp(app);
    weak_state = state;
    // Create Storage BEFORE AuthTokenProvider is attached.
    firebase::storage::Storage storage =
        firebase::storage::Storage::GetInstance(app, "gs://reverse-bucket");
    ASSERT_TRUE(storage.is_valid());

    auto provider_handle = state->GetOrCreateService<FakeAuthProvider>(
        &firebase::internal::kAuthServiceTag, "",
        [&raw_provider](firebase::internal::AppState&) {
          raw_provider = std::make_shared<FakeAuthProvider>();
          return raw_provider;
        });
    ASSERT_TRUE(provider_handle != nullptr);
    EXPECT_EQ(1, raw_provider->start_count.load());
    EXPECT_EQ(0, raw_provider->stop_count.load());
  }
  EXPECT_TRUE(weak_state.expired());
  ASSERT_TRUE(raw_provider != nullptr);
  EXPECT_EQ(1, raw_provider->start_count.load());
  EXPECT_EQ(1, raw_provider->stop_count.load());
}

TEST_F(StorageDesktopUtilsTests,
       UniformHandleOperatorsAndTemporaryReferenceAsyncCall) {
  std::weak_ptr<firebase::internal::AppState> weak_state;
  firebase::Future<firebase::storage::Metadata> metadata_future;
  {
    App app = firebase::testing::CreateApp(firebase::testing::MockAppOptions(),
                                           "storage_temp_async_app");
    weak_state = firebase::internal::AppState::FromApp(app);
    firebase::storage::Storage storage =
        firebase::storage::Storage::GetInstance(app, "gs://temp-bucket");
    ASSERT_TRUE(static_cast<bool>(storage));
    storage.UseEmulator("127.0.0.1", 1);
    storage.set_max_operation_retry_time(0.05);

    firebase::storage::StorageReference ref1 = storage.GetReference("a/b.txt");
    firebase::storage::StorageReference ref2 = storage.GetReference("a/b.txt");
    firebase::storage::StorageReference ref3 = storage.GetReference("a/c.txt");
    EXPECT_TRUE(static_cast<bool>(ref1));
    EXPECT_EQ(ref1, ref2);
    EXPECT_NE(ref1, ref3);
    firebase::storage::StorageReference ref_moved(std::move(ref2));
    EXPECT_FALSE(static_cast<bool>(ref2));  // NOLINT
    EXPECT_NE(ref_moved, ref2);             // NOLINT

    // Invoke an async call on a temporary StorageReference and immediately
    // drop both Storage and App handles while the request is in flight.
    metadata_future = storage.GetReference("tmp/object.txt").GetMetadata();
    EXPECT_NE(firebase::kFutureStatusInvalid, metadata_future.status());
  }

  EXPECT_TRUE(weak_state.expired());
  metadata_future.Wait(firebase::FutureBase::kWaitTimeoutInfinite);
  EXPECT_NE(firebase::kFutureStatusPending, metadata_future.status());
}

}  // namespace

int main(int argc, char** argv) {
  // On Linux, add: absl::SetFlag(&FLAGS_logtostderr, true);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
