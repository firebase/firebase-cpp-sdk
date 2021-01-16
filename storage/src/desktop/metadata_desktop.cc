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

#include "storage/src/desktop/metadata_desktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <iostream>
#include <sstream>
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

namespace firebase {
namespace storage {
namespace internal {

const int64_t kMillisToSeconds = 1000L;
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
      generation_(-1),
      metadata_generation_(-1),
      creation_time_(-1),
      updated_time_(-1),
      size_bytes_(-1) {
  UpdateStorageInternal();
  if (storage_reference_.is_valid()) {
    bucket_ = storage_reference_.bucket();
    path_ = storage_reference_.full_path();
    name_ = storage_reference_.name();
  }
}

MetadataInternal::MetadataInternal(const MetadataInternal& metadata) {
  *this = metadata;
}

MetadataInternal& MetadataInternal::operator=(
    const MetadataInternal& metadata) {
  storage_reference_ = metadata.storage_reference_;
  UpdateStorageInternal();
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
  custom_metadata_ = metadata.custom_metadata_;
  download_tokens_ = metadata.download_tokens_;
  download_url_ = metadata.download_url_;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
MetadataInternal::MetadataInternal(MetadataInternal&& other) {
  *this = std::move(other);
}

MetadataInternal& MetadataInternal::operator=(MetadataInternal&& other) {
  storage_reference_ = std::move(other.storage_reference_);
  other.storage_internal_ = nullptr;
  UpdateStorageInternal();
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
  custom_metadata_ = std::move(other.custom_metadata_);
  download_tokens_ = std::move(other.download_tokens_);
  download_url_ = std::move(other.download_url_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

const char* MetadataInternal::download_url() const {
  return download_url_.c_str();
}

std::string MetadataInternal::GetPathFromToken(const std::string& token) const {
  std::string http_url =
      StoragePath("gs://" + bucket_ + "/" + path_).AsHttpUrl();
  if (!token.empty()) http_url += "&token=" + token;
  return http_url;
}

void MetadataInternal::UpdateStorageInternal() {
  StorageReferenceInternal* internal = storage_reference_.internal_;
  storage_internal_ = internal ? internal->storage_internal() : nullptr;
}

StorageReferenceInternal* MetadataInternal::GetReference() const {
  return new StorageReferenceInternal(*storage_reference_.internal_);
}

std::string MetadataInternal::LookUpString(Variant* root, const char* key,
                                           const char* default_value) {
  std::map<Variant, Variant>::iterator lookup = root->map().find(key);
  return lookup != root->map().end() ? lookup->second.string_value()
                                     : default_value ? default_value : "";
}

int64_t MetadataInternal::LookUpInt64(Variant* root, const char* key) {
  std::map<Variant, Variant>::iterator lookup = root->map().find(key);
  return lookup != root->map().end() ? lookup->second.AsInt64().int64_value()
                                     : -1;
}

#if FIREBASE_PLATFORM_WINDOWS
// Use secure sscanf on Windows.
#define sscanf sscanf_s
#endif  // FIREBASE_PLATFORM_WINDOWS

// Times are saved in the metadata object as a string, in the following format:
// 2017-10-16T18:23:30.879Z
// This function takes such a string and returns the number of miliseconds
// since the epoch.  (or -1, if the string could not be parsed.)
// todo(ccornell) move to a static function and write unit tests. (b/69864585)
int64_t MetadataInternal::GetTimeFromTimeString(const std::string& time_str) {
  struct tm tm = {};
  int deciseconds = 0;
  if (sscanf(time_str.c_str(), "%d-%d-%dT%d:%d:%d.%dZ", &tm.tm_year, &tm.tm_mon,
             &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
             &deciseconds) != 7) {
    // Time string could not be parsed.
    return -1;
  }
  // We need to do some manual adjustments before we can actually use this data:
  // Months need to be zero-based.
  // Years need to be 1900-based.
  // isdst needs to be -1, to indicate that we don't know if we're in
  // daylight savings time or not.
  tm.tm_mon--;
  tm.tm_year -= 1900;
  tm.tm_isdst = -1;
  int milliseconds =
      deciseconds * (firebase::internal::kMillisecondsPerSecond / 10);
#if !FIREBASE_PLATFORM_WINDOWS
  return (timegm(&tm) * firebase::internal::kMillisecondsPerSecond) +
         milliseconds;
#else
  return (_mkgmtime(&tm) * firebase::internal::kMillisecondsPerSecond) +
         milliseconds;
#endif  // !FIREBASE_PLATFORM_WINDOWS
}

bool MetadataInternal::ImportFromJson(const char* json) {
  rest::util::JsonData data;
  if (!data.Parse(json)) return false;
  if (!data.root().is_map()) return false;

  Variant root = data.root();

  path_ = LookUpString(&root, kNameKey, path_.c_str());
  bucket_ = LookUpString(&root, kBucketKey, bucket_.c_str());
  // The server does not return the object name in the "name" field, it actually
  // returns the path so we derive the name from the path.
  name_ = Path(path_).GetBaseName();
  cache_control_ = LookUpString(&root, kCacheControlKey);
  content_type_ = LookUpString(&root, kContentTypeKey);

  generation_ = LookUpInt64(&root, kGenerationKey);
  metadata_generation_ = LookUpInt64(&root, kMetaGenerationKey);

  creation_time_ = GetTimeFromTimeString(LookUpString(&root, kTimeCreatedKey));
  updated_time_ = GetTimeFromTimeString(LookUpString(&root, kTimeUpdatedKey));

  size_bytes_ = LookUpInt64(&root, kSizeKey);
  md5_hash_ = LookUpString(&root, kMd5HashKey);
  content_disposition_ = LookUpString(&root, kContentDispositionKey);
  content_encoding_ = LookUpString(&root, kContentEncodingKey);
  content_language_ = LookUpString(&root, kContentLanguageKey);

  std::map<Variant, Variant>::iterator no_value = root.map().end();
  std::map<Variant, Variant>::iterator lookup;

  // Parse custom metadata.  (Stored as a map.)
  custom_metadata_.clear();
  lookup = root.map().find(Variant(kMetadataKey));
  if (lookup != no_value) {
    Variant json_metadata = root.map()[kMetadataKey];
    if (json_metadata.is_map()) {
      for (auto itr = json_metadata.map().begin();
           itr != json_metadata.map().end(); ++itr) {
        custom_metadata_[itr->first.string_value()] =
            itr->second.string_value();
      }
    }
  }

  // Parse download tokens.  (Stored as a comma-separated string list.)
  download_tokens_.clear();
  lookup = root.map().find(Variant(kDownloadTokensKey));
  if (lookup != no_value) {
    Variant json_tokens = root.map()[kDownloadTokensKey];
    if (json_tokens.is_string()) {
      std::istringstream token_list(json_tokens.string_value());
      std::string token;

      while (std::getline(token_list, token, ',')) {
        download_tokens_.push_back(token);
      }
    }
  }
  download_url_ = (!download_tokens_.empty())
                      ? GetPathFromToken(download_tokens_.front())
                      : "";
  return true;
}

std::string MetadataInternal::ExportAsJson() {
  Variant root = Variant::EmptyMap();

  std::map<std::string, Variant> map;
  std::map<Variant, Variant>& root_map = root.map();

  // We only export json members that have been set to non-default values.
  if (!name_.empty()) root_map[kNameKey] = name_;
  if (!bucket_.empty()) root_map[kBucketKey] = bucket_;
  if (generation_ != -1) root_map[kGenerationKey] = generation_;
  if (metadata_generation_ != -1) {
    root_map[kMetaGenerationKey] = metadata_generation_;
  }
  if (!content_type_.empty()) root_map[kContentTypeKey] = content_type_;

  // creation_time_/updated_time_ skipped, since we can't set this part of the
  // metadata directly anyway.

  if (size_bytes_ != -1) root_map[kSizeKey] = size_bytes_;
  if (!md5_hash_.empty()) root_map[kMd5HashKey] = kMd5HashKey;

  if (!content_encoding_.empty()) {
    root_map[kContentEncodingKey] = content_encoding_;
  }
  if (!content_disposition_.empty()) {
    root_map[kContentDispositionKey] = content_disposition_;
  }
  if (!content_language_.empty()) {
    root_map[kContentLanguageKey] = content_language_;
  }
  if (!cache_control_.empty()) {
    root_map[kCacheControlKey] = cache_control_;
  }

  // Make a nice comma-separated string for download tokens:
  bool first_in_list = true;
  std::string download_tokens;
  for (auto itr = download_tokens_.begin(); itr != download_tokens_.end();
       ++itr) {
    if (!first_in_list) download_tokens += ",";
    first_in_list = false;
    download_tokens += *itr;
  }
  if (!download_tokens.empty())
    root.map()[kDownloadTokensKey] = download_tokens;

  std::map<Variant, Variant> metadata_map;
  for (auto itr = custom_metadata_.begin(); itr != custom_metadata_.end();
       ++itr) {
    metadata_map[Variant(itr->first)] = Variant(itr->second);
  }
  if (!metadata_map.empty()) root.map()[kMetadataKey] = metadata_map;

  return util::VariantToJson(root);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
