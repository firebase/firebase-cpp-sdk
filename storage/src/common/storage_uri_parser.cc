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

#include <algorithm>
#include <cctype>
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
static const char kDomainBucketSeparator[] = "/b/";
// Path component that separates the bucket and path to the object.
static const char kBucketPathSeparator[] = "/o/";

// Strip the trailing slash from the specified string, leaving the string
// unmodified if it's not found.
static std::string StripTrailingSlash(const std::string& value) {
  if (value.empty()) return value;
  std::string result = value;
  while (!result.empty() && result.back() == '/') {
    result.pop_back();
  }
  return result;
}

static int HexDigitToInt(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string PercentDecode(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '%' && i + 2 < input.size()) {
      int hi = HexDigitToInt(input[i + 1]);
      int lo = HexDigitToInt(input[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    out.push_back(input[i]);
  }
  return out;
}

std::string PercentEncode(const std::string& input) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(input.size() * 3);
  for (unsigned char c : input) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
        c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(kHex[(c >> 4) & 0x0F]);
      out.push_back(kHex[c & 0x0F]);
    }
  }
  return out;
}

// Validate a URI scheme and extract the bucket / host and path.
bool UriToComponents(const std::string& url, const char* object_type,
                     std::string* bucket, std::string* path) {
  const char* safe_object_type = object_type ? object_type : "Storage";
  // Check the scheme.
  bool is_cloud_storage_scheme = false;
  const char* valid_scheme = nullptr;
  std::string valid_schemes;
  std::string scheme("(none)");
  std::string::size_type scheme_separator_index = url.find(kSchemeSeparator);
  if (scheme_separator_index != std::string::npos) {
    scheme_separator_index += sizeof(kSchemeSeparator) - 1;
    scheme = url.substr(0, scheme_separator_index);
    // URI schemes are case-insensitive (RFC 3986, section 3.1).
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char c) {
                     return static_cast<char>(std::tolower(c));
                   });
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
        safe_object_type, url.c_str(), scheme.c_str(), valid_schemes.c_str());
    return false;
  }
  std::string full_path(url.substr(scheme.length()));
  // Strip any query string or fragment from http(s) URLs before splitting
  // host/bucket/path. gs:// URIs are not URL-encoded, so '?' and '#' are
  // literal object-name characters there and must be preserved.
  if (!is_cloud_storage_scheme) {
    std::string::size_type query_or_frag = full_path.find_first_of("?#");
    if (query_or_frag != std::string::npos) {
      full_path.erase(query_or_frag);
    }
  }
  std::string::size_type it = full_path.find('/');
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
      // sizeof(kBucketPathSeparator) - 2 as we want to keep the leading
      // slash of the object path component.
      if (it != std::string::npos) {
        std::string encoded_object =
            remaining_path.substr(it + sizeof(kBucketPathSeparator) - 1);
        std::string decoded_object = PercentDecode(encoded_object);
        remaining_path =
            decoded_object.empty() ? std::string() : ("/" + decoded_object);
      } else {
        remaining_path = std::string();
      }
    } else {
      LogError("Unable to create %s from URL %s: missing bucket path.",
               safe_object_type, url.c_str());
      return false;
    }
  }
  if (parsed_bucket.empty() ||
      parsed_bucket.find('/') != std::string::npos) {
    LogError("Unable to create %s from URL %s: bucket name is empty or invalid.",
             safe_object_type, url.c_str());
    return false;
  }
  if (bucket) *bucket = parsed_bucket;
  if (path) *path = StripTrailingSlash(remaining_path);
  return true;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
