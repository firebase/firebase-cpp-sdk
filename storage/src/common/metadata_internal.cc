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

#include "storage/src/common/metadata_internal.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "app/rest/util.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/path.h"
#include "app/src/time.h"
#include "app/src/variant_util.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

const char* MetadataInternal::kContentLanguageKey = "contentLanguage";
const char* MetadataInternal::kContentEncodingKey = "contentEncoding";
const char* MetadataInternal::kContentDispositionKey = "contentDisposition";
const char* MetadataInternal::kCacheControlKey = "cacheControl";
const char* MetadataInternal::kMetadataKey = "metadata";
const char* MetadataInternal::kContentTypeKey = "contentType";
const char* MetadataInternal::kDownloadTokensKey = "downloadTokens";
const char* MetadataInternal::kMd5HashKey = "md5Hash";
const char* MetadataInternal::kSizeKey = "size";
const char* MetadataInternal::kTimeUpdatedKey = "updated";
const char* MetadataInternal::kTimeCreatedKey = "timeCreated";
const char* MetadataInternal::kMetaGenerationKey = "metageneration";
const char* MetadataInternal::kBucketKey = "bucket";
const char* MetadataInternal::kNameKey = "name";
const char* MetadataInternal::kGenerationKey = "generation";

MetadataInternal::MetadataInternal(const StorageReference& storage_reference)
    : storage_reference_(storage_reference),
      path_(storage_reference.full_path()),
      name_(storage_reference.name()),
      bucket_(storage_reference.bucket()) {}

MetadataInternal::MetadataInternal(std::nullptr_t) : storage_reference_(nullptr) {}

MetadataInternal::MetadataInternal(const MetadataInternal& metadata)
    : storage_reference_(metadata.storage_reference_),
      path_(metadata.path_),
      name_(metadata.name_),
      bucket_(metadata.bucket_),
      cache_control_(metadata.cache_control_),
      content_type_(metadata.content_type_),
      generation_(metadata.generation_),
      metadata_generation_(metadata.metadata_generation_),
      creation_time_(metadata.creation_time_),
      updated_time_(metadata.updated_time_),
      size_bytes_(metadata.size_bytes_),
      md5_hash_(metadata.md5_hash_),
      content_disposition_(metadata.content_disposition_),
      content_encoding_(metadata.content_encoding_),
      content_language_(metadata.content_language_),
      has_cache_control_(metadata.has_cache_control_),
      has_content_disposition_(metadata.has_content_disposition_),
      has_content_encoding_(metadata.has_content_encoding_),
      has_content_language_(metadata.has_content_language_),
      has_content_type_(metadata.has_content_type_),
      custom_metadata_(metadata.custom_metadata_),
      download_tokens_(metadata.download_tokens_) {
}

MetadataInternal& MetadataInternal::operator=(
    const MetadataInternal& metadata) {
  if (this == &metadata) {
    return *this;
  }
  storage_reference_ = metadata.storage_reference_;
  name_ = metadata.name_;
  path_ = metadata.path_;
  bucket_ = metadata.bucket_;
  cache_control_ = metadata.cache_control_;
  content_type_ = metadata.content_type_;
  generation_ = metadata.generation_;
  metadata_generation_ = metadata.metadata_generation_;
  creation_time_ = metadata.creation_time_;
  updated_time_ = metadata.updated_time_;
  size_bytes_ = metadata.size_bytes_;
  md5_hash_ = metadata.md5_hash_;
  content_disposition_ = metadata.content_disposition_;
  content_encoding_ = metadata.content_encoding_;
  content_language_ = metadata.content_language_;
  has_cache_control_ = metadata.has_cache_control_;
  has_content_disposition_ = metadata.has_content_disposition_;
  has_content_encoding_ = metadata.has_content_encoding_;
  has_content_language_ = metadata.has_content_language_;
  has_content_type_ = metadata.has_content_type_;
  custom_metadata_ = metadata.custom_metadata_;
  download_tokens_ = metadata.download_tokens_;
  return *this;
}

MetadataInternal::MetadataInternal(MetadataInternal&& other) noexcept
    : storage_reference_(std::move(other.storage_reference_)),
      path_(std::move(other.path_)),
      name_(std::move(other.name_)),
      bucket_(std::move(other.bucket_)),
      cache_control_(std::move(other.cache_control_)),
      content_type_(std::move(other.content_type_)),
      generation_(other.generation_),
      metadata_generation_(other.metadata_generation_),
      creation_time_(other.creation_time_),
      updated_time_(other.updated_time_),
      size_bytes_(other.size_bytes_),
      md5_hash_(std::move(other.md5_hash_)),
      content_disposition_(std::move(other.content_disposition_)),
      content_encoding_(std::move(other.content_encoding_)),
      content_language_(std::move(other.content_language_)),
      has_cache_control_(other.has_cache_control_),
      has_content_disposition_(other.has_content_disposition_),
      has_content_encoding_(other.has_content_encoding_),
      has_content_language_(other.has_content_language_),
      has_content_type_(other.has_content_type_),
      custom_metadata_(std::move(other.custom_metadata_)),
      download_tokens_(std::move(other.download_tokens_)) {
}

MetadataInternal& MetadataInternal::operator=(
    MetadataInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  storage_reference_ = std::move(other.storage_reference_);
  name_ = std::move(other.name_);
  path_ = std::move(other.path_);
  bucket_ = std::move(other.bucket_);
  cache_control_ = std::move(other.cache_control_);
  content_type_ = std::move(other.content_type_);
  generation_ = other.generation_;
  metadata_generation_ = other.metadata_generation_;
  creation_time_ = other.creation_time_;
  updated_time_ = other.updated_time_;
  size_bytes_ = other.size_bytes_;
  md5_hash_ = std::move(other.md5_hash_);
  content_disposition_ = std::move(other.content_disposition_);
  content_encoding_ = std::move(other.content_encoding_);
  content_language_ = std::move(other.content_language_);
  has_cache_control_ = other.has_cache_control_;
  has_content_disposition_ = other.has_content_disposition_;
  has_content_encoding_ = other.has_content_encoding_;
  has_content_language_ = other.has_content_language_;
  has_content_type_ = other.has_content_type_;
  custom_metadata_ = std::move(other.custom_metadata_);
  download_tokens_ = std::move(other.download_tokens_);
  return *this;
}









std::string MetadataInternal::LookUpString(const Variant& root, const char* key,
                                           const char* default_value) {
  const auto& map = root.map();
  auto lookup = map.find(Variant(key));
  if (lookup != map.end() && lookup->second.is_string()) {
    return lookup->second.string_value();
  }
  return default_value ? default_value : "";
}

bool MetadataInternal::LookUpOptionalString(const Variant& root,
                                            const char* key,
                                            std::string* value) {
  const auto& map = root.map();
  auto lookup = map.find(Variant(key));
  if (lookup != map.end() && lookup->second.is_string()) {
    *value = lookup->second.string_value();
    return true;
  }
  value->clear();
  return false;
}

int64_t MetadataInternal::LookUpInt64(const Variant& root, const char* key) {
  const auto& map = root.map();
  auto lookup = map.find(Variant(key));
  if (lookup != map.end()) {
    Variant as_int = lookup->second.AsInt64();
    if (as_int.is_int64()) {
      return as_int.int64_value();
    }
  }
  return -1;
}

// Times are saved in the metadata object as an RFC 3339 string, e.g.:
// 2017-10-16T18:23:30.879Z or 2017-10-16T18:23:30Z or with +/-HH:MM offset.
// This function takes such a string and returns the number of milliseconds
// since the epoch (or -1, if the string could not be parsed).
int64_t MetadataInternal::GetTimeFromTimeString(const std::string& time_str) {
  // Minimum RFC 3339 timestamp "YYYY-MM-DDTHH:MM:SSZ" has 20 characters.
  if (time_str.size() < 20) {
    return -1;
  }
  const auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
  if (!is_digit(time_str[0]) || !is_digit(time_str[1]) ||
      !is_digit(time_str[2]) || !is_digit(time_str[3]) || time_str[4] != '-' ||
      !is_digit(time_str[5]) || !is_digit(time_str[6]) || time_str[7] != '-' ||
      !is_digit(time_str[8]) || !is_digit(time_str[9]) ||
      (time_str[10] != 'T' && time_str[10] != 't') ||
      !is_digit(time_str[11]) || !is_digit(time_str[12]) ||
      time_str[13] != ':' || !is_digit(time_str[14]) ||
      !is_digit(time_str[15]) || time_str[16] != ':' ||
      !is_digit(time_str[17]) || !is_digit(time_str[18])) {
    return -1;
  }

  struct tm tm = {};
  int chars_consumed = 0;
  if (std::sscanf(time_str.c_str(), "%4d-%2d-%2d%*1[Tt]%2d:%2d:%2d%n",
                  &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min,
                  &tm.tm_sec, &chars_consumed) < 6 ||
      chars_consumed != 19) {
    return -1;
  }
  if (tm.tm_year < 1970 || tm.tm_year > 9999 || tm.tm_mon < 1 ||
      tm.tm_mon > 12 || tm.tm_mday < 1 || tm.tm_mday > 31 || tm.tm_hour < 0 ||
      tm.tm_hour > 23 || tm.tm_min < 0 || tm.tm_min > 59 || tm.tm_sec < 0 ||
      tm.tm_sec > 60) {
    return -1;
  }

  const char* p = time_str.c_str() + chars_consumed;
  int64_t milliseconds = 0;
  if (*p == '.') {
    ++p;
    if (!is_digit(*p)) {
      return -1;
    }
    int frac_digits = 0;
    int64_t frac_value = 0;
    int64_t frac_scale = 1000;
    while (is_digit(*p)) {
      if (frac_digits < 3) {
        frac_value = frac_value * 10 + (*p - '0');
        frac_scale /= 10;
      }
      ++frac_digits;
      ++p;
    }
    milliseconds = frac_value * frac_scale;
  }

  int64_t tz_offset_ms = 0;
  if (*p == 'Z' || *p == 'z') {
    ++p;
  } else if (*p == '+' || *p == '-') {
    int sign = (*p == '+') ? 1 : -1;
    ++p;
    if (!is_digit(p[0]) || !is_digit(p[1]) || p[2] != ':' || !is_digit(p[3]) ||
        !is_digit(p[4])) {
      return -1;
    }
    int tz_hour = (p[0] - '0') * 10 + (p[1] - '0');
    int tz_min = (p[3] - '0') * 10 + (p[4] - '0');
    if (tz_hour < 0 || tz_hour > 23 || tz_min < 0 || tz_min > 59) {
      return -1;
    }
    p += 5;
    tz_offset_ms =
        sign * ((static_cast<int64_t>(tz_hour) * 3600 +
                 static_cast<int64_t>(tz_min) * 60) *
                firebase::internal::kMillisecondsPerSecond);
  } else {
    return -1;
  }

  if (*p != '\0') {
    return -1;
  }

  tm.tm_mon--;
  tm.tm_year -= 1900;
  tm.tm_isdst = -1;
#if !FIREBASE_PLATFORM_WINDOWS
  time_t epoch_sec = timegm(&tm);
#else
  time_t epoch_sec = _mkgmtime(&tm);
#endif  // !FIREBASE_PLATFORM_WINDOWS
  if (epoch_sec == static_cast<time_t>(-1)) {
    return -1;
  }
  return (static_cast<int64_t>(epoch_sec) *
          firebase::internal::kMillisecondsPerSecond) +
         milliseconds - tz_offset_ms;
}

bool MetadataInternal::ImportFromJson(const char* json) {
  if (!json) return false;
  rest::util::JsonData data;
  if (!data.Parse(json)) return false;
  if (!data.root().is_map()) return false;

  const Variant& root = data.root();

  // The server returns the full object path in the "name" field, whereas
  // ExportAsJson() exports the object's basename in "name". Preserve path_ if
  // the imported "name" is only the basename matching the existing path_.
  const std::string imported_name =
      LookUpString(root, kNameKey, path_.c_str());
  if (path_.empty() || imported_name.find('/') != std::string::npos ||
      Path(path_).GetBaseName() != imported_name) {
    path_ = imported_name;
  }
  bucket_ = LookUpString(root, kBucketKey, bucket_.c_str());
  name_ = Path(path_).GetBaseName();
  // Every settable field is overwritten from the JSON, so its has_* flag must
  // track whether the server actually returned it; otherwise a stale flag
  // from a previous Set*()/import on a reused object would make
  // ExportAsJson() emit a field the server omitted.
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
    if (!tokens.empty()) {
      download_tokens_.push_back(tokens);
    }
  }

  size_bytes_ = LookUpInt64(root, kSizeKey);
  md5_hash_ = LookUpString(root, kMd5HashKey);
  has_content_disposition_ = LookUpOptionalString(
      root, kContentDispositionKey, &content_disposition_);
  has_content_encoding_ =
      LookUpOptionalString(root, kContentEncodingKey, &content_encoding_);
  has_content_language_ =
      LookUpOptionalString(root, kContentLanguageKey, &content_language_);

  const auto& root_map = root.map();

  // Parse custom metadata.  (Stored as a map.)
  custom_metadata_.clear();
  auto metadata_lookup = root_map.find(Variant(kMetadataKey));
  if (metadata_lookup != root_map.end() && metadata_lookup->second.is_map()) {
    const auto& json_metadata_map = metadata_lookup->second.map();
    for (const auto& entry : json_metadata_map) {
      if (entry.first.is_string()) {
        if (entry.second.is_string()) {
          custom_metadata_[entry.first.string_value()] =
              entry.second.string_value();
        } else {
          Variant as_str = entry.second.AsString();
          if (as_str.is_string()) {
            custom_metadata_[entry.first.string_value()] =
                as_str.string_value();
          }
        }
      }
    }
  }

  return true;
}

std::string MetadataInternal::ExportAsJson() const {
  Variant root = Variant::EmptyMap();

  std::map<Variant, Variant>& root_map = root.map();

  // We only export json members that have been set to non-default values,
  // or explicitly cleared via a setter.
  if (!name_.empty()) root_map[kNameKey] = name_;
  if (!bucket_.empty()) root_map[kBucketKey] = bucket_;
  if (generation_ != -1) root_map[kGenerationKey] = generation_;
  if (metadata_generation_ != -1) {
    root_map[kMetaGenerationKey] = metadata_generation_;
  }
  if (has_content_type_ || !content_type_.empty()) {
    root_map[kContentTypeKey] = content_type_;
  }

  // creation_time_/updated_time_ skipped, since we can't set this part of the
  // metadata directly anyway.

  if (size_bytes_ != -1) root_map[kSizeKey] = size_bytes_;
  if (!md5_hash_.empty()) root_map[kMd5HashKey] = md5_hash_;

  if (has_content_encoding_ || !content_encoding_.empty()) {
    root_map[kContentEncodingKey] = content_encoding_;
  }
  if (has_content_disposition_ || !content_disposition_.empty()) {
    root_map[kContentDispositionKey] = content_disposition_;
  }
  if (has_content_language_ || !content_language_.empty()) {
    root_map[kContentLanguageKey] = content_language_;
  }
  if (has_cache_control_ || !cache_control_.empty()) {
    root_map[kCacheControlKey] = cache_control_;
  }


  std::map<Variant, Variant> metadata_map;
  for (const auto& entry : custom_metadata_) {
    metadata_map[Variant(entry.first)] = Variant(entry.second);
  }
  if (!metadata_map.empty()) root_map[kMetadataKey] = metadata_map;

  return util::VariantToJson(root);
}

}  // namespace firebase::storage::internal
