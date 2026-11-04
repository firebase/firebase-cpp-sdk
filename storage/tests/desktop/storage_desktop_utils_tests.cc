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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/rest/util.h"
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "storage/src/common/list_result_internal.h"
#include "storage/src/desktop/callback_dispatcher.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/listener_desktop.h"
#include "storage/src/common/metadata_internal.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/include/firebase/storage/storage_reference.h"
#include "testing/json_util.h"

namespace {

using firebase::App;
using firebase::Future;
using firebase::storage::Controller;
using firebase::storage::Error;
using firebase::storage::GetErrorMessage;
using firebase::storage::Listener;
using firebase::storage::Metadata;
using firebase::storage::Storage;
using firebase::storage::StorageListResult;
using firebase::storage::StorageReference;
using firebase::storage::internal::ControllerInternal;
using firebase::storage::internal::MetadataInternal;
using firebase::storage::internal::StorageInternal;
using firebase::storage::internal::StorageListResultInternal;
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
  test_path = StoragePath(
      nullptr,
      "http://firebasestorage.googleapis.com/v0/b/Bucket/o/"
      "path%2f%2f%2f%2fto%2FObject%2f%2f%2f%2fObject.data");
  EXPECT_STREQ(test_path.GetBucket().c_str(), intended_bucket_result.c_str());
  EXPECT_STREQ(test_path.GetPath().c_str(), intended_path_result.c_str());

  // With query string and fragment:
  test_path = StoragePath(
      nullptr,
      "https://firebasestorage.googleapis.com/v0/b/Bucket/o/"
      "path%2Fto%2FObject%2FObject.data?alt=media&token=abc#section");
  EXPECT_STREQ(test_path.GetBucket().c_str(), intended_bucket_result.c_str());
  EXPECT_STREQ(test_path.GetPath().c_str(), intended_path_result.c_str());
}

TEST_F(StorageDesktopUtilsTests, testInvalidConstructors) {
  StoragePath bad_path(nullptr, "argleblargle://Bucket/path1/path2/Object");
  EXPECT_FALSE(bad_path.IsValid());

  StoragePath empty_gs_bucket(nullptr, "gs:///path/Object");
  EXPECT_FALSE(empty_gs_bucket.IsValid());

  StoragePath empty_http_bucket(
      nullptr, "https://firebasestorage.googleapis.com/v0/b//o/Object");
  EXPECT_FALSE(empty_http_bucket.IsValid());
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
  std::unique_ptr<App> app(firebase::testing::CreateApp());
  std::unique_ptr<StorageInternal> storage(
      new StorageInternal(app.get(), "gs://Bucket"));

  StoragePath test_path(storage.get(), "gs://Bucket/path1/path2/Object");

  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path1/path2/Object");

  EXPECT_STREQ(test_path.AsHttpUrl().c_str(),
               "https://firebasestorage.googleapis.com:443"
               "/v0/b/Bucket/o/path1%2Fpath2%2FObject?alt=media");
  EXPECT_STREQ(test_path.AsHttpMetadataUrl().c_str(),
               "https://firebasestorage.googleapis.com:443"
               "/v0/b/Bucket/o/path1%2Fpath2%2FObject");

  // Verify UseEmulator changes scheme, host, and port.
  storage->UseEmulator("127.0.0.1", 9199);
  EXPECT_STREQ(test_path.AsHttpUrl().c_str(),
               "http://127.0.0.1:9199"
               "/v0/b/Bucket/o/path1%2Fpath2%2FObject?alt=media");
  EXPECT_STREQ(test_path.AsHttpMetadataUrl().c_str(),
               "http://127.0.0.1:9199"
               "/v0/b/Bucket/o/path1%2Fpath2%2FObject");
}

TEST_F(StorageDesktopUtilsTests, testMetadataJsonExporter) {
  std::unique_ptr<App> app(firebase::testing::CreateApp());
  std::unique_ptr<StorageInternal> storage(
      new StorageInternal(app.get(), "gs://abucket"));
  std::unique_ptr<StorageReferenceInternal> reference(
      storage->GetReferenceFromUrl("gs://abucket/path/to/a/file.txt"));
  MetadataInternal metadata(reference->AsStorageReference());
  reference.reset(nullptr);

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

  // Explicitly clearing fields should export empty strings in JSON so the
  // server clears them.
  metadata.set_cache_control("");
  metadata.set_content_disposition(nullptr);
  std::string cleared_json = metadata.ExportAsJson();
  EXPECT_NE(cleared_json.find("\"cacheControl\":\"\""), std::string::npos);
  EXPECT_NE(cleared_json.find("\"contentDisposition\":\"\""),
            std::string::npos);
}

TEST_F(StorageDesktopUtilsTests, testMetadataJsonImporterAndTimestamps) {
  std::unique_ptr<App> app(firebase::testing::CreateApp());
  std::unique_ptr<StorageInternal> storage(
      new StorageInternal(app.get(), "gs://abucket"));
  std::unique_ptr<StorageReferenceInternal> reference(
      storage->GetReferenceFromUrl("gs://abucket/dir/file.txt"));
  MetadataInternal metadata(reference->AsStorageReference());

  const char* kJson =
      "{"
      "  \"name\": \"dir/file.txt\","
      "  \"bucket\": \"abucket\","
      "  \"generation\": \"12345\","
      "  \"metageneration\": \"6\","
      "  \"contentType\": \"text/plain\","
      "  \"timeCreated\": \"2017-10-16T18:23:30.879Z\","
      "  \"updated\": \"2017-10-16T20:23:30.879123+02:00\","
      "  \"size\": \"4096\","
      "  \"md5Hash\": \"rL0Y20zC+Fzt72VPzMSk2A==\","
      "  \"contentEncoding\": \"gzip\","
      "  \"contentDisposition\": \"inline\","
      "  \"contentLanguage\": \"en-US\","
      "  \"cacheControl\": \"max-age=3600\","
      "  \"downloadTokens\": \"tok1,tok2\","
      "  \"metadata\": {\"strKey\": \"strVal\", \"numKey\": 42}"
      "}";
  EXPECT_TRUE(metadata.ImportFromJson(kJson));
  EXPECT_STREQ(metadata.name(), "file.txt");
  EXPECT_STREQ(metadata.path(), "dir/file.txt");
  EXPECT_STREQ(metadata.bucket(), "abucket");
  EXPECT_EQ(metadata.generation(), 12345);
  EXPECT_EQ(metadata.metadata_generation(), 6);
  EXPECT_STREQ(metadata.content_type(), "text/plain");
  EXPECT_EQ(metadata.creation_time(), 1508178210879LL);
  EXPECT_EQ(metadata.updated_time(), 1508178210879LL);
  EXPECT_EQ(metadata.size_bytes(), 4096);
  EXPECT_STREQ(metadata.md5_hash(), "rL0Y20zC+Fzt72VPzMSk2A==");
  EXPECT_STREQ(metadata.content_encoding(), "gzip");
  EXPECT_STREQ(metadata.content_disposition(), "inline");
  EXPECT_STREQ(metadata.content_language(), "en-US");
  EXPECT_STREQ(metadata.cache_control(), "max-age=3600");
  EXPECT_EQ((*metadata.custom_metadata())["strKey"], "strVal");
  EXPECT_EQ((*metadata.custom_metadata())["numKey"], "42");
}

TEST_F(StorageDesktopUtilsTests, testPublicApiValueSemanticsAndValidation) {
  // Error messages
  EXPECT_STREQ(GetErrorMessage(firebase::storage::kErrorNone),
               "The operation was a success, no error occurred");
  EXPECT_STREQ(GetErrorMessage(firebase::storage::kErrorCancelled),
               "User cancelled the operation");
  EXPECT_STREQ(GetErrorMessage(firebase::storage::kErrorInvalidArgument),
               "An invalid argument was provided");
  EXPECT_STREQ(GetErrorMessage(static_cast<Error>(999)), "Unknown error.");

  // Controller value semantics
  Controller c1;
  EXPECT_FALSE(c1.is_valid());
  EXPECT_EQ(c1.total_byte_count(), -1);
  EXPECT_EQ(c1.bytes_transferred(), 0);
  EXPECT_FALSE(c1.is_paused());
  EXPECT_FALSE(c1.Pause());
  EXPECT_FALSE(c1.Resume());
  EXPECT_FALSE(c1.Cancel());
  Controller c2(c1);
  EXPECT_EQ(c1, c2);
  Controller& c1_alias = c1;  // Self-assignment without -Wself-assign.
  c1 = c1_alias;
  Controller c3(std::move(c2));
  EXPECT_FALSE(c3.is_valid());

  // Metadata value semantics
  Metadata m1;
  EXPECT_TRUE(m1.is_valid());
  m1.set_content_type("application/json");
  (*m1.custom_metadata())["k"] = "v";
  Metadata m2(m1);
  EXPECT_EQ(m1, m2);
  m2.set_content_type("text/html");
  EXPECT_NE(m1, m2);
  Metadata& m1_alias = m1;  // Self-assignment without -Wself-assign.
  m1 = m1_alias;
  EXPECT_STREQ(m1.content_type(), "application/json");
  Metadata m3(std::move(m2));
  EXPECT_TRUE(m3.is_valid());
  EXPECT_STREQ(m3.content_type(), "text/html");

  // Storage & StorageReference navigation and input validation
  std::unique_ptr<App> app(firebase::testing::CreateApp());
  firebase::InitResult init_result = firebase::kInitResultFailedMissingDependency;
  std::unique_ptr<Storage> storage(
      Storage::GetInstance(app.get(), "gs://test-bucket", &init_result));
  ASSERT_NE(storage, nullptr);
  EXPECT_EQ(init_result, firebase::kInitResultSuccess);

  StorageReference root = storage->GetReference();
  EXPECT_TRUE(root.is_valid());
  EXPECT_TRUE(root.is_root());
  EXPECT_FALSE(root.GetParent().is_valid());
  EXPECT_EQ(root.ToString(), "gs://test-bucket");

  StorageReference child = root.Child("folder/item.bin");
  EXPECT_TRUE(child.is_valid());
  EXPECT_FALSE(child.is_root());
  EXPECT_EQ(child.bucket(), "test-bucket");
  EXPECT_EQ(child.full_path(), "/folder/item.bin");
  EXPECT_EQ(child.name(), "item.bin");
  EXPECT_EQ(child.ToString(), "gs://test-bucket/folder/item.bin");
  EXPECT_EQ(child.GetRoot(), root);
  EXPECT_EQ(child.GetParent().full_path(), "/folder");
  EXPECT_EQ(child.GetParent().GetParent(), root);

  // Invalid argument futures complete immediately with kErrorInvalidArgument
  Future<StorageListResult> bad_list = child.List(0);
  EXPECT_EQ(bad_list.status(), firebase::kFutureStatusComplete);
  EXPECT_EQ(bad_list.error(), firebase::storage::kErrorInvalidArgument);

  Future<StorageListResult> bad_list_large = child.List(1001);
  EXPECT_EQ(bad_list_large.status(), firebase::kFutureStatusComplete);
  EXPECT_EQ(bad_list_large.error(), firebase::storage::kErrorInvalidArgument);

  Future<size_t> bad_get_bytes = child.GetBytes(nullptr, 16);
  EXPECT_EQ(bad_get_bytes.status(), firebase::kFutureStatusComplete);
  EXPECT_EQ(bad_get_bytes.error(), firebase::storage::kErrorInvalidArgument);

  Future<Metadata> bad_put_bytes = child.PutBytes(nullptr, 16);
  EXPECT_EQ(bad_put_bytes.status(), firebase::kFutureStatusComplete);
  EXPECT_EQ(bad_put_bytes.error(), firebase::storage::kErrorInvalidArgument);

  Future<size_t> bad_get_file = child.GetFile("");
  EXPECT_EQ(bad_get_file.status(), firebase::kFutureStatusComplete);
  EXPECT_EQ(bad_get_file.error(), firebase::storage::kErrorInvalidArgument);

  Future<Metadata> bad_put_file = child.PutFile("");
  EXPECT_EQ(bad_put_file.status(), firebase::kFutureStatusComplete);
  EXPECT_EQ(bad_put_file.error(), firebase::storage::kErrorInvalidArgument);

  // StorageListResult value semantics
  StorageListResult empty_list;
  EXPECT_FALSE(empty_list.is_valid());
  StorageListResult valid_list = StorageListResultInternal::AsStorageListResult(
      new StorageListResultInternal({child.GetParent()}, {child}, "next-tok"));
  EXPECT_TRUE(valid_list.is_valid());
  EXPECT_EQ(valid_list.prefixes().size(), 1u);
  EXPECT_EQ(valid_list.items().size(), 1u);
  EXPECT_EQ(valid_list.next_page_token(), "next-tok");
  StorageListResult copied_list(valid_list);
  EXPECT_TRUE(copied_list.is_valid());
  EXPECT_EQ(copied_list.next_page_token(), "next-tok");
}

TEST_F(StorageDesktopUtilsTests, testCallbackDispatcherOrderingAndShutdown) {
  firebase::storage::internal::CallbackDispatcher dispatcher;
  std::vector<int> order;
  std::promise<void> done;
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(dispatcher.Post([&order, i]() { order.push_back(i); }));
  }
  EXPECT_TRUE(dispatcher.Post([&done, &dispatcher]() {
    EXPECT_TRUE(dispatcher.IsWorkerThread());
    done.set_value();
  }));
  done.get_future().wait();
  EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4}));
  EXPECT_FALSE(dispatcher.IsWorkerThread());
  dispatcher.Shutdown();
  EXPECT_FALSE(dispatcher.Post([]() {}));
  dispatcher.Shutdown();  // Idempotent.
}

TEST_F(StorageDesktopUtilsTests, testCallbackDispatcherDestroyedFromCallback) {
  // A callback may destroy its own dispatcher (as when a Listener deletes its
  // Storage): the worker detaches instead of joining itself.
  auto* dispatcher = new firebase::storage::internal::CallbackDispatcher();
  std::promise<void> destroyed;
  ASSERT_TRUE(dispatcher->Post([dispatcher, &destroyed]() {
    delete dispatcher;
    destroyed.set_value();
  }));
  EXPECT_EQ(destroyed.get_future().wait_for(std::chrono::seconds(10)),
            std::future_status::ready);
}

TEST_F(StorageDesktopUtilsTests, testStorageReferenceHandlesAcrossStorageDeletion) {
  std::unique_ptr<App> app(firebase::testing::CreateApp());
  firebase::InitResult init_result;
  Storage* storage =
      Storage::GetInstance(app.get(), "gs://handle-bucket", &init_result);
  ASSERT_NE(storage, nullptr);
  StorageReference root = storage->GetReference();
  StorageReference child = root.Child("a/b.txt");
  StorageReference copied(child);
  StorageReference moved(std::move(copied));
  EXPECT_FALSE(copied.is_valid());
  EXPECT_TRUE(moved.is_valid());
  StorageReference assigned;
  assigned = moved;
  EXPECT_EQ(assigned.full_path(), "/a/b.txt");
  StorageReference move_assigned;
  move_assigned = std::move(assigned);
  EXPECT_FALSE(assigned.is_valid());
  EXPECT_EQ(move_assigned.full_path(), "/a/b.txt");
  {
    // Released while the Storage is alive: unregistered and deleted.
    StorageReference temp = root.Child("temp");
    EXPECT_TRUE(temp.is_valid());
  }

  delete storage;
  // Every surviving handle was invalidated by the Storage's cleanup, and can
  // still be copied, moved and destroyed safely.
  EXPECT_FALSE(root.is_valid());
  EXPECT_FALSE(child.is_valid());
  EXPECT_FALSE(moved.is_valid());
  EXPECT_FALSE(move_assigned.is_valid());
  StorageReference after_copy(child);
  StorageReference after_move(std::move(moved));
  EXPECT_FALSE(after_copy.is_valid());
  EXPECT_FALSE(after_move.is_valid());
}

}  // namespace
