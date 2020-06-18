// Copyright 2018 Google LLC
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

#include "storage/src/common/storage_uri_parser.h"

#include <cstddef>

#include "app/src/include/firebase/internal/common.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace storage {
namespace internal {

struct UriAndComponents {
  // URI to parse.
  const char* path;
  // Expected bucket from URI.
  const char* expected_bucket;
  // Expected path from URI.
  const char* expected_path;
};

TEST(StorageUriParserTest, TestInvalidUris) {
  EXPECT_FALSE(UriToComponents("", "test", nullptr, nullptr));
  EXPECT_FALSE(UriToComponents("invalid://uri", "test", nullptr, nullptr));
}

TEST(StorageUriParserTest, TestValidUris) {
  EXPECT_TRUE(
      UriToComponents("gs://somebucket", "gs_scheme", nullptr, nullptr));
  EXPECT_TRUE(UriToComponents("http://domain/b/bucket", "http_scheme", nullptr,
                              nullptr));
  EXPECT_TRUE(UriToComponents("https://domain/b/bucket",  // NOTYPO
                              "http_scheme", nullptr, nullptr));
}

// Extract components from each URI in uri_and_expected_components and compare
// with the expectedBucket & expectedPath values in the specified structure.
// object_prefix is used as a prefix for the object name supplied to each
// call to UriToComponents() to aid debugging when an error is reported by the
// method.
static void ExtractComponents(
    const UriAndComponents* uri_and_expected_components,
    size_t number_of_uri_and_expected_components,
    const std::string& object_prefix) {
  for (size_t i = 0; i < number_of_uri_and_expected_components; ++i) {
    const auto& param = uri_and_expected_components[i];
    {
      std::string bucket;
      EXPECT_TRUE(UriToComponents(
          param.path, (object_prefix + "_bucket").c_str(), &bucket, nullptr));
      EXPECT_EQ(param.expected_bucket, bucket);
    }
    {
      std::string path;
      EXPECT_TRUE(UriToComponents(param.path, (object_prefix + "_path").c_str(),
                                  nullptr, &path));
      EXPECT_EQ(param.expected_path, path);
    }
    {
      std::string bucket;
      std::string path;
      EXPECT_TRUE(UriToComponents(param.path, (object_prefix + "_all").c_str(),
                                  &bucket, &path));
      EXPECT_EQ(param.expected_bucket, bucket);
      EXPECT_EQ(param.expected_path, path);
    }
  }
}

TEST(StorageUriParserTest, TestExtractGsSchemeComponents) {
  const UriAndComponents kTestParams[] = {
      {
          "gs://somebucket",
          "somebucket",
          "",
      },
      {
          "gs://somebucket/",
          "somebucket",
          "",
      },
      {
          "gs://somebucket/a/path/to/an/object",
          "somebucket",
          "/a/path/to/an/object",
      },
      {
          "gs://somebucket/a/path/to/an/object/",
          "somebucket",
          "/a/path/to/an/object",
      },
  };
  ExtractComponents(kTestParams, FIREBASE_ARRAYSIZE(kTestParams), "gsscheme");
}

TEST(StorageUriParserTest, TestExtractHttpHttpsSchemeComponents) {
  const UriAndComponents kTestParams[] = {
      {
          "http://firebasestorage.googleapis.com/v0/b/somebucket",
          "somebucket",
          "",
      },
      {
          "http://firebasestorage.googleapis.com/v0/b/somebucket/",
          "somebucket",
          "",
      },
      {
          "http://firebasestorage.googleapis.com/v0/b/somebucket/o/an/object",
          "somebucket",
          "/an/object",
      },
      {
          "http://firebasestorage.googleapis.com/v0/b/somebucket/o/an/object/",
          "somebucket",
          "/an/object",
      },
      {
          "https://firebasestorage.googleapis.com/v0/b/somebucket/",
          "somebucket",
          "",
      },
      {
          "https://firebasestorage.googleapis.com/v0/b/somebucket/o/an/object",
          "somebucket",
          "/an/object",
      },
      {
          "https://firebasestorage.googleapis.com/v0/b/somebucket/o/an/object/",
          "somebucket",
          "/an/object",
      },
  };
  ExtractComponents(kTestParams, FIREBASE_ARRAYSIZE(kTestParams), "http(s)");
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
