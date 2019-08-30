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

const char kBucketStartString[] = "firebasestorage.googleapis.com/v0/b/";
const size_t kBucketStartStringLength = FIREBASE_STRLEN(kBucketStartString);
const char kBucketEndString[] = "/o/";
const size_t kBucketEndStringLength = FIREBASE_STRLEN(kBucketEndString);

StoragePath::StoragePath(const std::string& path) {
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
StoragePath::StoragePath(const std::string& bucket, const std::string& path,
                         const std::string& object) {
  bucket_ = bucket;
  path_ = Path(path).GetChild(object);
}

// URIs have the format:  "gs://<appname>.appspot.com/path/to/object"
void StoragePath::ConstructFromGsUri(const std::string& uri, int path_start) {
  std::string bucket_and_path = uri.substr(path_start);
  std::string::size_type first_slash_pos = bucket_and_path.find(kSeparator);
  bucket_ = bucket_and_path.substr(0, first_slash_pos);
  path_ = Path(first_slash_pos == std::string::npos
                   ? ""
                   : bucket_and_path.substr(first_slash_pos));
}

// HTTP URLs the format:
// http[s]://firebasestorage.googleapis.com/v0/b/<bucket>/o/<path/to/object>
// Note that slashes in the path need to be html-encoded.
void StoragePath::ConstructFromHttpUrl(const std::string& url, int path_start) {
  std::string path = url.substr(path_start, url.length() - path_start);

  std::string::size_type bucket_start = path.find(kBucketStartString);
  std::string::size_type bucket_end = path.rfind(kBucketEndString);

  if (bucket_start == std::string::npos || bucket_end == std::string::npos) {
    return;
  }

  bucket_start += kBucketStartStringLength;
  size_t object_start = bucket_end + kBucketEndStringLength;
  bucket_ = path.substr(bucket_start, bucket_end - bucket_start);
  path_ = Path(rest::util::DecodeUrl(path.substr(object_start)));
}

// Returns the contents of this object as a URL to the storage REST endpoint.
// Is guaranteed to have a query string, so more arguments can be freely
// added as necessary.
std::string StoragePath::AsHttpUrl() const {
  static const char* kUrlEnd = "?alt=media";
  // Construct the URL.  Final format is:
  // https://[projectname].googleapis.com/v0/b/[bucket]/o/[path and/or object]
  return AsHttpMetadataUrl() + kUrlEnd;
}

std::string StoragePath::AsHttpMetadataUrl() const {
  // Construct the URL.  Final format is:
  // https://[projectname].googleapis.com/v0/b/[bucket]/o/[path and/or object]
  std::string result = kHttpsScheme;
  result += kBucketStartString;
  result += bucket_;
  result += kBucketEndString;
  result += rest::util::EncodeUrl(path_.str());
  return result;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
