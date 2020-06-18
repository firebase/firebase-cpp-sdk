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
#include <memory>

#include "app/rest/util.h"
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/metadata_desktop.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/desktop/storage_reference_desktop.h"
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
  test_path = StoragePath("gs://Bucket/path/Object");

  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/Object");

  // Test a more complex path:
  test_path = StoragePath("gs://Bucket/path/morepath/Object");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/morepath/Object");

  // Extra slashes:
  test_path = StoragePath("gs://Bucket/path////Object");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/Object");

  // Path with no Object:
  test_path = StoragePath("gs://Bucket/path////more////");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/more");
}

// Test the HTTP(S)-based StoragePath constructors
TEST_F(StorageDesktopUtilsTests, testHTTPStoragePathConstructors) {
  StoragePath test_path;
  std::string intended_bucket_result = "Bucket";
  std::string intended_path_result = "path/to/Object/Object.data";

  // Test basic case:
  test_path = StoragePath(
      "http://firebasestorage.googleapis.com/v0/b/Bucket/o/"
      "path%2fto%2FObject%2fObject.data");
  EXPECT_STREQ(test_path.GetBucket().c_str(), intended_bucket_result.c_str());
  EXPECT_STREQ(test_path.GetPath().c_str(), intended_path_result.c_str());

  // httpS (instead of http):
  test_path = StoragePath(
      "https://firebasestorage.googleapis.com/v0/b/Bucket/o/"
      "path%2fto%2FObject%2fObject.data");
  EXPECT_STREQ(test_path.GetBucket().c_str(), intended_bucket_result.c_str());
  EXPECT_STREQ(test_path.GetPath().c_str(), intended_path_result.c_str());

  // Extra slashes:
  test_path = StoragePath(
      "http://firebasestorage.googleapis.com/v0/b/Bucket/o/"
      "path%2f%2f%2f%2fto%2FObject%2f%2f%2f%2fObject.data");
  EXPECT_STREQ(test_path.GetBucket().c_str(), intended_bucket_result.c_str());
  EXPECT_STREQ(test_path.GetPath().c_str(), intended_path_result.c_str());
}

TEST_F(StorageDesktopUtilsTests, testInvalidConstructors) {
  StoragePath bad_path("argleblargle://Bucket/path1/path2/Object");
  EXPECT_FALSE(bad_path.IsValid());
}

// Test the StoragePath.Parent() function.
TEST_F(StorageDesktopUtilsTests, testStoragePathParent) {
  StoragePath test_path;

  // Test parent, when there is an GetObject.
  test_path = StoragePath("gs://Bucket/path/Object").GetParent();
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path");

  // Test parent with no GetObject.
  test_path = StoragePath("gs://Bucket/path/morepath/").GetParent();
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path");
}

// Test the StoragePath.Child() function.
TEST_F(StorageDesktopUtilsTests, testStoragePathChild) {
  StoragePath test_path;

  // Test child when there is no object.
  test_path = StoragePath("gs://Bucket/path/morepath/").GetChild("newobj");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/morepath/newobj");

  // Test child when there is an object.
  test_path = StoragePath("gs://Bucket/path/object").GetChild("newpath/");
  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path/object/newpath");
}

TEST_F(StorageDesktopUtilsTests, testUrlConverter) {
  StoragePath test_path("gs://Bucket/path1/path2/Object");

  EXPECT_STREQ(test_path.GetBucket().c_str(), "Bucket");
  EXPECT_STREQ(test_path.GetPath().c_str(), "path1/path2/Object");

  EXPECT_STREQ(test_path.AsHttpUrl().c_str(),
               "https://firebasestorage.googleapis.com"
               "/v0/b/Bucket/o/path1%2Fpath2%2FObject?alt=media");
  EXPECT_STREQ(test_path.AsHttpMetadataUrl().c_str(),
               "https://firebasestorage.googleapis.com"
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
  EXPECT_THAT(
      json,
      ::firebase::testing::cppsdk::EqualsJson(
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

}  // namespace

int main(int argc, char** argv) {
  // On Linux, add: absl::SetFlag(&FLAGS_logtostderr, true);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
