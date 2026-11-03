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

#include "storage/src/desktop/storage_path.h"

#include <string.h>

#include <string>

#include "app/rest/util.h"
#include "app/src/include/firebase/internal/common.h"
#include "storage/src/desktop/storage_desktop.h"

namespace firebase {
namespace storage {
namespace internal {

const char* const StoragePath::kSeparator = "/";

const char kGsScheme[] = "gs://";
const size_t kGsSchemeLength = FIREBASE_STRLEN(kGsScheme);
const char kHttpScheme[] = "http://";
const size_t kHttpSchemeLength = FIREBASE_STRLEN(kHttpScheme);
const char kHttpsScheme[] = "https://";
const size_t kHttpsSchemeLength = FIREBASE_STRLEN(kHttpsScheme);

const char kBucketEndString[] = "/o/";
const size_t kBucketEndStringLength = FIREBASE_STRLEN(kBucketEndString);
const char kBucketIdentifierString[] = "/v0/b/";
const size_t kBucketIdentifierStringLength =
    FIREBASE_STRLEN(kBucketIdentifierString);

StoragePath::StoragePath(StorageInternal* storage, const std::string& path) {
  storage_internal_ = storage;
  bucket_ = "";
  path_ = Path("");
  if (path.compare(0, kGsSchemeLength, kGsScheme) == 0) {
    ConstructFromGsUri(path, kGsSchemeLength);
  } else if (path.compare(0, kHttpSchemeLength, kHttpScheme) == 0) {
    ConstructFromHttpUrl(path, kHttpSchemeLength);
  } else if (path.compare(0, kHttpsSchemeLength, kHttpsScheme) == 0) {
    ConstructFromHttpUrl(path, kHttpsSchemeLength);
  } else {
    // Error!  Invalid path!
    return;
  }
}

// Constructs a storage path, based on raw strings for the bucket, path, and
// object.
StoragePath::StoragePath(StorageInternal* storage, const std::string& bucket,
                         const std::string& path, const std::string& object) {
  storage_internal_ = storage;
  bucket_ = bucket;
  path_ = Path(path).GetChild(object);
}

// URIs have the format:  "gs://<appname>.appspot.com/path/to/object"
// gs:// URIs are not URL-encoded and have no query or fragment component, so
// '?' and '#' are literal characters of the object name and are kept as is.
void StoragePath::ConstructFromGsUri(const std::string& uri, int path_start) {
  const std::string bucket_and_path = uri.substr(path_start);
  std::string::size_type first_slash_pos = bucket_and_path.find(kSeparator);
  bucket_ = bucket_and_path.substr(0, first_slash_pos);
  path_ = Path(first_slash_pos == std::string::npos
                   ? ""
                   : bucket_and_path.substr(first_slash_pos));
}

// HTTP URLs have the format:
// http[s]://<host>/v0/b/<bucket>/o/<path/to/object>[?query][#fragment]
// Note that slashes in the path need to be URL-encoded.
void StoragePath::ConstructFromHttpUrl(const std::string& url, int path_start) {
  std::string path = url.substr(path_start);
  std::string::size_type query_or_frag = path.find_first_of("?#");
  if (query_or_frag != std::string::npos) {
    path = path.substr(0, query_or_frag);
  }

  std::string::size_type bucket_start = path.find(kBucketIdentifierString);
  if (bucket_start == std::string::npos || bucket_start == 0) {
    return;
  }
  bucket_start += kBucketIdentifierStringLength;

  std::string::size_type bucket_end =
      path.find(kBucketEndString, bucket_start);
  std::string object_encoded;
  if (bucket_end != std::string::npos) {
    object_encoded = path.substr(bucket_end + kBucketEndStringLength);
  } else if (path.size() >= bucket_start + 2 &&
             path.compare(path.size() - 2, 2, "/o") == 0) {
    bucket_end = path.size() - 2;
  } else {
    bucket_end = path.find('/', bucket_start);
    if (bucket_end == std::string::npos) {
      bucket_end = path.size();
    } else if (bucket_end + 1 < path.size()) {
      return;
    }
  }

  std::string parsed_bucket =
      path.substr(bucket_start, bucket_end - bucket_start);
  if (parsed_bucket.empty() ||
      parsed_bucket.find('/') != std::string::npos) {
    return;
  }
  bucket_ = parsed_bucket;
  path_ = Path(rest::util::DecodeUrl(object_encoded));
}

// Returns the contents of this object as a URL to the storage REST endpoint.
// Is guaranteed to have a query string, so more arguments can be freely
// added as necessary.
std::string StoragePath::AsHttpUrl() const {
  if (!storage_internal_) return "";
  static const char* kUrlEnd = "?alt=media";
  // Construct the URL.  Final format is:
  // http[s]://[host]:[port]/v0/b/[bucket]/o/[path and/or object]
  return AsHttpMetadataUrl() + kUrlEnd;
}

std::string StoragePath::AsHttpMetadataUrl() const {
  if (!storage_internal_) return "";
  // Construct the URL.  Final format is:
  // [scheme]://[host]:[port]/v0/b/[bucket]/o/[path and/or object]

  std::string result = storage_internal_->get_scheme();
  result += "://";
  result += storage_internal_->get_host();
  result += ":";
  result += std::to_string(storage_internal_->get_port());
  result += kBucketIdentifierString;
  result += bucket_;
  result += kBucketEndString;
  result += rest::util::EncodeUrl(path_.str());
  return result;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
