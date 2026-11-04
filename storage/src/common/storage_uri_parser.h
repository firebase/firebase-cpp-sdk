// Copyright 2018 Google LLC
#ifndef FIREBASE_STORAGE_SRC_COMMON_STORAGE_URI_PARSER_H_
#define FIREBASE_STORAGE_SRC_COMMON_STORAGE_URI_PARSER_H_

#include <string>

namespace firebase::storage::internal {

extern const char* kCloudStorageScheme;
std::string PercentDecode(const std::string& input);
std::string PercentEncode(const std::string& input);
bool UriToComponents(const std::string& url, const char* object_type,
                     std::string* bucket, std::string* path);
inline bool UriToComponents(const char* url, const char* object_type,
                            std::string* bucket, std::string* path) {
  return url && UriToComponents(std::string(url), object_type, bucket, path);
}

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_COMMON_STORAGE_URI_PARSER_H_
