// Copyright 2017 Google LLC
#include "storage/src/common/metadata_internal.h"

#if defined(FIREBASE_TARGET_DESKTOP)
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>

#include "app/rest/util.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/path.h"
#include "app/src/time.h"
#include "app/src/variant_util.h"

namespace firebase::storage::internal {

std::string MetadataInternal::LookUpString(const Variant& root, const char* key,
                                           const char* default_value) {
  const auto& map = root.map();
  auto it = map.find(Variant(key));
  return (it != map.end() && it->second.is_string())
             ? it->second.string_value()
             : (default_value ? default_value : "");
}

bool MetadataInternal::LookUpOptionalString(const Variant& root,
                                            const char* key,
                                            std::string* value) {
  const auto& map = root.map();
  auto it = map.find(Variant(key));
  if (it != map.end() && it->second.is_string()) {
    *value = it->second.string_value();
    return true;
  }
  value->clear();
  return false;
}

int64_t MetadataInternal::LookUpInt64(const Variant& root, const char* key) {
  const auto& map = root.map();
  auto it = map.find(Variant(key));
  if (it != map.end()) {
    Variant as_int = it->second.AsInt64();
    if (as_int.is_int64()) return as_int.int64_value();
  }
  return -1;
}

int64_t MetadataInternal::GetTimeFromTimeString(const std::string& s) {
  if (s.size() < 20) return -1;
  const auto is_d = [](char c) { return c >= '0' && c <= '9'; };
  for (int i : {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18}) {
    if (!is_d(s[i])) return -1;
  }
  if (s[4] != '-' || s[7] != '-' || (s[10] != 'T' && s[10] != 't') ||
      s[13] != ':' || s[16] != ':') {
    return -1;
  }
  struct tm tm = {};
  int consumed = 0;
  if (std::sscanf(s.c_str(), "%4d-%2d-%2d%*1[Tt]%2d:%2d:%2d%n", &tm.tm_year,
                  &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
                  &consumed) < 6 ||
      consumed != 19 || tm.tm_year < 1970 || tm.tm_year > 9999 ||
      tm.tm_mon < 1 || tm.tm_mon > 12 || tm.tm_mday < 1 || tm.tm_mday > 31 ||
      tm.tm_hour < 0 || tm.tm_hour > 23 || tm.tm_min < 0 || tm.tm_min > 59 ||
      tm.tm_sec < 0 || tm.tm_sec > 60) {
    return -1;
  }
  const char* p = s.c_str() + consumed;
  int64_t ms = 0;
  if (*p == '.') {
    if (!is_d(*++p)) return -1;
    int digits = 0;
    int64_t val = 0, scale = 1000;
    while (is_d(*p)) {
      if (digits++ < 3) {
        val = val * 10 + (*p - '0');
        scale /= 10;
      }
      ++p;
    }
    ms = val * scale;
  }
  int64_t tz_offset_ms = 0;
  if (*p == 'Z' || *p == 'z') {
    ++p;
  } else if (*p == '+' || *p == '-') {
    int sign = (*p++ == '+') ? 1 : -1;
    if (!is_d(p[0]) || !is_d(p[1]) || p[2] != ':' || !is_d(p[3]) ||
        !is_d(p[4])) {
      return -1;
    }
    int tz_h = (p[0] - '0') * 10 + (p[1] - '0');
    int tz_m = (p[3] - '0') * 10 + (p[4] - '0');
    if (tz_h > 23 || tz_m > 59) return -1;
    p += 5;
    tz_offset_ms =
        sign * (static_cast<int64_t>(tz_h) * 3600 + tz_m * 60) *
        firebase::internal::kMillisecondsPerSecond;
  } else {
    return -1;
  }
  if (*p != '\0') return -1;
  tm.tm_mon--;
  tm.tm_year -= 1900;
  tm.tm_isdst = -1;
#if !FIREBASE_PLATFORM_WINDOWS
  time_t epoch_sec = timegm(&tm);
#else
  time_t epoch_sec = _mkgmtime(&tm);
#endif
  if (epoch_sec == static_cast<time_t>(-1)) return -1;
  return static_cast<int64_t>(epoch_sec) *
             firebase::internal::kMillisecondsPerSecond +
         ms - tz_offset_ms;
}

bool MetadataInternal::ImportFromJson(const char* json) {
  if (!json) return false;
  rest::util::JsonData data;
  if (!data.Parse(json) || !data.root().is_map()) return false;
  const Variant& root = data.root();
  const std::string imported_name =
      LookUpString(root, kNameKey, path_.c_str());
  if (path_.empty() || imported_name.find('/') != std::string::npos ||
      Path(path_).GetBaseName() != imported_name) {
    path_ = imported_name;
  }
  bucket_ = LookUpString(root, kBucketKey, bucket_.c_str());
  name_ = Path(path_).GetBaseName();
  has_cache_control_ =
      LookUpOptionalString(root, kCacheControlKey, &cache_control_);
  has_content_type_ =
      LookUpOptionalString(root, kContentTypeKey, &content_type_);
  generation_ = LookUpInt64(root, kGenerationKey);
  metadata_generation_ = LookUpInt64(root, kMetaGenerationKey);
  creation_time_ = GetTimeFromTimeString(LookUpString(root, kTimeCreatedKey));
  updated_time_ = GetTimeFromTimeString(LookUpString(root, kTimeUpdatedKey));

  download_tokens_.clear();
  std::string tokens = LookUpString(root, kDownloadTokensKey);
  if (!tokens.empty()) {
    size_t pos = 0;
    while ((pos = tokens.find(',')) != std::string::npos) {
      download_tokens_.push_back(tokens.substr(0, pos));
      tokens.erase(0, pos + 1);
    }
    if (!tokens.empty()) download_tokens_.push_back(tokens);
  }

  size_bytes_ = LookUpInt64(root, kSizeKey);
  md5_hash_ = LookUpString(root, kMd5HashKey);
  has_content_disposition_ = LookUpOptionalString(
      root, kContentDispositionKey, &content_disposition_);
  has_content_encoding_ =
      LookUpOptionalString(root, kContentEncodingKey, &content_encoding_);
  has_content_language_ =
      LookUpOptionalString(root, kContentLanguageKey, &content_language_);

  custom_metadata_.clear();
  const auto& root_map = root.map();
  auto it = root_map.find(Variant(kMetadataKey));
  if (it != root_map.end() && it->second.is_map()) {
    for (const auto& entry : it->second.map()) {
      if (entry.first.is_string()) {
        Variant val = entry.second.is_string() ? entry.second
                                               : entry.second.AsString();
        if (val.is_string()) {
          custom_metadata_[entry.first.string_value()] = val.string_value();
        }
      }
    }
  }
  return true;
}

std::string MetadataInternal::ExportAsJson() const {
  Variant root = Variant::EmptyMap();
  auto& m = root.map();
  if (!name_.empty()) m[kNameKey] = name_;
  if (!bucket_.empty()) m[kBucketKey] = bucket_;
  if (generation_ != -1) m[kGenerationKey] = generation_;
  if (metadata_generation_ != -1) m[kMetaGenerationKey] = metadata_generation_;
  if (has_content_type_ || !content_type_.empty()) {
    m[kContentTypeKey] = content_type_;
  }
  if (size_bytes_ != -1) m[kSizeKey] = size_bytes_;
  if (!md5_hash_.empty()) m[kMd5HashKey] = md5_hash_;
  if (has_content_encoding_ || !content_encoding_.empty()) {
    m[kContentEncodingKey] = content_encoding_;
  }
  if (has_content_disposition_ || !content_disposition_.empty()) {
    m[kContentDispositionKey] = content_disposition_;
  }
  if (has_content_language_ || !content_language_.empty()) {
    m[kContentLanguageKey] = content_language_;
  }
  if (has_cache_control_ || !cache_control_.empty()) {
    m[kCacheControlKey] = cache_control_;
  }
  if (!custom_metadata_.empty()) {
    std::map<Variant, Variant> meta;
    for (const auto& [k, v] : custom_metadata_) meta[Variant(k)] = Variant(v);
    m[kMetadataKey] = meta;
  }
  return util::VariantToJson(root);
}

}  // namespace firebase::storage::internal
#endif  // defined(FIREBASE_TARGET_DESKTOP)
