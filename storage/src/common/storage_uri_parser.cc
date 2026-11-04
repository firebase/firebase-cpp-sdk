// Copyright 2018 Google LLC
#include "storage/src/common/storage_uri_parser.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"

namespace firebase::storage::internal {

const char* kCloudStorageScheme = "gs://";

namespace {
constexpr const char kSchemeSeparator[] = "://";
constexpr const char kDomainBucketSeparator[] = "/b/";
constexpr const char kBucketPathSeparator[] = "/o/";

std::string StripTrailingSlash(std::string v) {
  while (!v.empty() && v.back() == '/') v.pop_back();
  return v;
}

int HexDigitToInt(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

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
  static constexpr const char kHex[] = "0123456789ABCDEF";
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

bool UriToComponents(const std::string& url, const char* object_type,
                     std::string* bucket, std::string* path) {
  const char* safe_type = object_type ? object_type : "Storage";
  std::string scheme("(none)");
  size_t sep = url.find(kSchemeSeparator);
  if (sep != std::string::npos) {
    scheme = url.substr(0, sep + sizeof(kSchemeSeparator) - 1);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char c) {
                     return static_cast<char>(std::tolower(c));
                   });
  }
  const bool is_gs = (scheme == kCloudStorageScheme);
  if (!is_gs && scheme != "http://" && scheme != "https://") {
    LogError("Unable to create %s from URL %s with scheme %s. "
             "URL should start with one of (gs://|http://|https://).",
             safe_type, url.c_str(), scheme.c_str());
    return false;
  }

  std::string full_path = url.substr(scheme.length());
  if (!is_gs) {
    size_t q = full_path.find_first_of("?#");
    if (q != std::string::npos) full_path.erase(q);
  }
  size_t slash = full_path.find('/');
  std::string host =
      slash != std::string::npos ? full_path.substr(0, slash) : full_path;
  std::string rem = full_path.substr(host.length());
  std::string parsed_bucket;
  if (is_gs) {
    parsed_bucket = host;
  } else {
    size_t b_pos = rem.find(kDomainBucketSeparator);
    if (b_pos == std::string::npos) {
      LogError("Unable to create %s from URL %s: missing bucket path.",
               safe_type, url.c_str());
      return false;
    }
    rem = rem.substr(b_pos + sizeof(kDomainBucketSeparator) - 1);
    size_t o_pos = rem.find(kBucketPathSeparator);
    parsed_bucket = StripTrailingSlash(rem.substr(0, o_pos));
    if (o_pos != std::string::npos) {
      std::string decoded =
          PercentDecode(rem.substr(o_pos + sizeof(kBucketPathSeparator) - 1));
      rem = decoded.empty() ? std::string() : ("/" + decoded);
    } else {
      rem.clear();
    }
  }
  if (parsed_bucket.empty() || parsed_bucket.find('/') != std::string::npos) {
    LogError("Unable to create %s from URL %s: bucket name is empty or invalid.",
             safe_type, url.c_str());
    return false;
  }
  if (bucket) *bucket = parsed_bucket;
  if (path) *path = StripTrailingSlash(rem);
  return true;
}

}  // namespace firebase::storage::internal
