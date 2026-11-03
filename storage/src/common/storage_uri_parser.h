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

#ifndef FIREBASE_STORAGE_SRC_COMMON_STORAGE_URI_PARSER_H_
#define FIREBASE_STORAGE_SRC_COMMON_STORAGE_URI_PARSER_H_

#include <string>

namespace firebase {
namespace storage {
namespace internal {

extern const char* kCloudStorageScheme;

// Decode percent-encoded (%XX) sequences in a URI component.
std::string PercentDecode(const std::string& input);

// Percent-encode a URI object path component per RFC 3986 (leaving
// unreserved characters A-Z, a-z, 0-9, '-', '.', '_', '~' unencoded).
std::string PercentEncode(const std::string& input);

// Validate a URI scheme and extract the bucket / host and path.
bool UriToComponents(const std::string& url, const char* object_type,
                     std::string* bucket, std::string* path);

inline bool UriToComponents(const char* url, const char* object_type,
                            std::string* bucket, std::string* path) {
  if (!url) return false;
  return UriToComponents(std::string(url), object_type, bucket, path);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_COMMON_STORAGE_URI_PARSER_H_
