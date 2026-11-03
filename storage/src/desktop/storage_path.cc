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

#include <string>
#include <string_view>

#include "app/rest/util.h"
#include "storage/src/desktop/storage_desktop.h"

namespace firebase::storage::internal {

namespace {

constexpr std::string_view kHttpScheme = "http://";
constexpr std::string_view kHttpsScheme = "https://";

constexpr std::string_view kBucketEndString = "/o/";
constexpr std::string_view kBucketIdentifierString = "/v0/b/";
constexpr std::string_view kBucketOnlySuffix = "/o";

constexpr bool StartsWith(std::string_view str, std::string_view prefix) {
  return str.substr(0, prefix.size()) == prefix;
}

constexpr bool EndsWith(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size() &&
         str.substr(str.size() - suffix.size()) == suffix;
}

}  // namespace

const char* const StoragePath::kSeparator = "/";

const char kGsScheme[] = "gs://";

StoragePath::StoragePath(StorageInternal* storage, const std::string& path)
    : storage_internal_(storage) {
  const std::string_view uri(path);
  const std::string_view gs_scheme(kGsScheme);
  if (StartsWith(uri, gs_scheme)) {
    ConstructFromGsUri(uri.substr(gs_scheme.size()));
  } else if (StartsWith(uri, kHttpScheme)) {
    ConstructFromHttpUrl(uri.substr(kHttpScheme.size()));
  } else if (StartsWith(uri, kHttpsScheme)) {
    ConstructFromHttpUrl(uri.substr(kHttpsScheme.size()));
  }
  // Otherwise the path is invalid and is left empty (IsValid() is false).
}

// Constructs a storage path, based on raw strings for the bucket, path, and
// object.
StoragePath::StoragePath(StorageInternal* storage, const std::string& bucket,
                         const std::string& path, const std::string& object)
    : bucket_(bucket),
      path_(Path(path).GetChild(object)),
      storage_internal_(storage) {}

// URIs have the format:  "gs://<appname>.appspot.com/path/to/object"
// gs:// URIs are not URL-encoded and have no query or fragment component, so
// '?' and '#' are literal characters of the object name and are kept as is.
// `bucket_and_path` is the URI with the scheme already stripped.
void StoragePath::ConstructFromGsUri(std::string_view bucket_and_path) {
  const std::string_view::size_type first_slash_pos =
      bucket_and_path.find('/');
  bucket_ = std::string(bucket_and_path.substr(0, first_slash_pos));
  path_ = Path(first_slash_pos == std::string_view::npos
                   ? std::string()
                   : std::string(bucket_and_path.substr(first_slash_pos)));
}

// HTTP URLs have the format:
// http[s]://<host>/v0/b/<bucket>/o/<path/to/object>[?query][#fragment]
// Note that slashes in the path need to be URL-encoded.
// `url` is the URL with the scheme already stripped. It is parsed entirely
// through string_views; only the final bucket and decoded object are copied.
void StoragePath::ConstructFromHttpUrl(std::string_view url) {
  // Drop the query and fragment, if any.
  const std::string_view path = url.substr(0, url.find_first_of("?#"));

  std::string_view::size_type bucket_start =
      path.find(kBucketIdentifierString);
  if (bucket_start == std::string_view::npos || bucket_start == 0) {
    return;
  }
  bucket_start += kBucketIdentifierString.size();

  std::string_view::size_type bucket_end =
      path.find(kBucketEndString, bucket_start);
  std::string_view object_encoded;
  if (bucket_end != std::string_view::npos) {
    object_encoded = path.substr(bucket_end + kBucketEndString.size());
  } else if (path.size() >= bucket_start + kBucketOnlySuffix.size() &&
             EndsWith(path, kBucketOnlySuffix)) {
    bucket_end = path.size() - kBucketOnlySuffix.size();
  } else {
    bucket_end = path.find('/', bucket_start);
    if (bucket_end == std::string_view::npos) {
      bucket_end = path.size();
    } else if (bucket_end + 1 < path.size()) {
      return;
    }
  }

  const std::string_view parsed_bucket =
      path.substr(bucket_start, bucket_end - bucket_start);
  if (parsed_bucket.empty() ||
      parsed_bucket.find('/') != std::string_view::npos) {
    return;
  }
  bucket_ = std::string(parsed_bucket);
  path_ = Path(rest::util::DecodeUrl(std::string(object_encoded)));
}

// Returns the contents of this object as a URL to the storage REST endpoint.
// Is guaranteed to have a query string, so more arguments can be freely
// added as necessary.
std::string StoragePath::AsHttpUrl() const {
  if (!storage_internal_) return "";
  constexpr std::string_view kUrlEnd = "?alt=media";
  // Construct the URL.  Final format is:
  // http[s]://[host]:[port]/v0/b/[bucket]/o/[path and/or object]
  return AsHttpMetadataUrl().append(kUrlEnd);
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

}  // namespace firebase::storage::internal
