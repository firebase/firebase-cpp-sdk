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

#include <string>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"

namespace firebase {
namespace storage {
namespace internal {

const char* kCloudStorageScheme = "gs://";

static const char kSchemeSeparator[] = "://";
static const char kHttpScheme[] = "http://";
static const char kHttpsScheme[] = "https://";
static const char* kValidSchemes[] = {
    kCloudStorageScheme,
    kHttpScheme,
    kHttpsScheme,
};

// http / https paths are in the following format:
// [scheme]://[projectname].googleapis.com/v0/b/[bucket]/o/[path and/or object]
//
// Path component that separates the domain and bucket in http / https paths.
static const char kDomainBucketSeparator[] = "/v0/b/";
// Path component that separates the bucket and path to the object.
static const char kBucketPathSeparator[] = "/o/";

// Strip the trailing slash from the specified string, leaving the string
// unmodified if it's not found.
static std::string StripTrailingSlash(const std::string& value) {
  auto it = value.rfind("/");
  return it == value.length() - 1 ? value.substr(0, it) : value;
}

// Validate a URI scheme and extract the bucket / host and path.
bool UriToComponents(const std::string& url, const char* object_type,
                     std::string* bucket, std::string* path) {
  // Check the scheme.
  bool is_cloud_storage_scheme = false;
  const char* valid_scheme = nullptr;
  std::string valid_schemes;
  std::string scheme("(none)");
  std::string::size_type scheme_separator_index = url.find(kSchemeSeparator);
  if (scheme_separator_index != std::string::npos) {
    scheme_separator_index += sizeof(kSchemeSeparator) - 1;
    scheme = url.substr(0, scheme_separator_index);
  }

  size_t number_of_valid_schemes = FIREBASE_ARRAYSIZE(kValidSchemes);
  for (size_t i = 0; i < number_of_valid_schemes && !valid_scheme; ++i) {
    const char* current_scheme = kValidSchemes[i];
    if (scheme.compare(current_scheme) == 0) {
      valid_scheme = current_scheme;
      is_cloud_storage_scheme = current_scheme == kCloudStorageScheme;
    }
    valid_schemes.append(current_scheme);
    if (i < number_of_valid_schemes - 1) valid_schemes.append("|");
  }

  if (!valid_scheme) {
    LogError(
        "Unable to create %s from URL %s with scheme %s. "
        "URL should start with one of (%s).",
        object_type, url.c_str(), scheme.c_str(), valid_schemes.c_str());
    return false;
  }
  std::string full_path(url.substr(scheme.length()));
  std::string::size_type it = full_path.find("/");
  std::string host(full_path);
  if (it != std::string::npos) host = full_path.substr(0, it);
  std::string remaining_path(full_path.substr(host.length()));
  std::string parsed_bucket;
  if (is_cloud_storage_scheme) {
    parsed_bucket = host;
  } else {
    // Find the start of the bucket component.
    it = remaining_path.find(kDomainBucketSeparator);
    if (it != std::string::npos) {
      // Find the end of the bucket component.
      remaining_path =
          remaining_path.substr(it + sizeof(kDomainBucketSeparator) - 1);
      it = remaining_path.find(kBucketPathSeparator);
      // Extract the bucket component.
      parsed_bucket = StripTrailingSlash(remaining_path.substr(0, it));
      // Remove all components up to the object path separator.
      // sizeof(kBucketPathSeparator) - 2 as need want to keep the leading
      // slash of the object path component.
      remaining_path =
          it != std::string::npos
              ? remaining_path.substr(it + sizeof(kBucketPathSeparator) - 2)
              : std::string();
    } else {
      remaining_path = std::string();
    }
  }
  if (bucket) *bucket = parsed_bucket;
  if (path) *path = StripTrailingSlash(remaining_path);
  return true;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
