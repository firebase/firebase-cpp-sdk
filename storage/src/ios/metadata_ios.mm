// Copyright 2016 Google LLC
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

#include "storage/src/ios/metadata_ios.h"

#import <Foundation/Foundation.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#import "FirebaseStorage-Swift.h"
#include "storage/src/ios/storage_ios.h"

namespace firebase {
namespace storage {
namespace internal {

// Create a copy of a FIRStorageMetadata if non-nil.
static FIRStorageMetadata* _Nullable CopyObjCMetadataObject(
    const FIRStorageMetadata* _Nullable src) {
  return src ? [src copy] : nil;
}

// Convert a setter argument to the NSString to store, or nil to clear the
// property. firebase/storage/metadata.h documents the string setters as
// "nullptr/empty to clear", matching the Android and desktop implementations,
// so nullptr and "" map to nil (the field is omitted from the uploaded
// metadata) rather than to @"". A value that is not valid UTF-8 cannot be
// represented and also clears the field.
static NSString* _Nullable NSStringOrNilFromSetterValue(
    const char* _Nullable c_string) {
  if (!c_string || c_string[0] == '\0') {
    return nil;
  }
  return [NSString stringWithUTF8String:c_string];
}

// Convert an NSString to a cached UTF-8 C-string without mutating or
// reallocating the backing std::string when its value is unchanged.
static const char* _Nonnull NSStringToCString(
    NSString* _Nullable nsstring_value, std::string* _Nonnull string_slot) {
  const char* utf8_string =
      (nsstring_value && nsstring_value.UTF8String) ? nsstring_value.UTF8String
                                                    : "";
  if (*string_slot != utf8_string) {
    *string_slot = utf8_string;
  }
  return string_slot->c_str();
}

// Convert NSDate to milliseconds since the epoch, or -1 if nil.
//
// firebase/storage/metadata.h documents creation_time() and updated_time() as
// returning "-1 if unset or invalid", which is also what the desktop and
// Android implementations return, so an unset date maps to -1, not 0.
static int64_t NSDateToMilliseconds(NSDate* _Nullable date) {
  if (!date) {
    return -1;
  }
  double seconds_since_the_epoch = date.timeIntervalSince1970;
  int64_t milliseconds_since_the_epoch =
      static_cast<int64_t>(seconds_since_the_epoch * 1000.0);
  return milliseconds_since_the_epoch;
}

// Replaces the contents of `out` with the custom metadata stored in `impl`.
static void LoadCustomMetadata(FIRStorageMetadata* _Nullable impl,
                               CustomMetadata* _Nonnull out) {
  out->clear();
  NSDictionary<NSString*, NSString*>* dictionary =
      impl ? impl.customMetadata : nil;
  for (NSString* key in dictionary) {
    const char* key_cstr = key ? key.UTF8String : nullptr;
    if (!key_cstr) continue;
    NSString* value = [dictionary objectForKey:key];
    const char* val_cstr = value ? value.UTF8String : nullptr;
    out->insert(std::make_pair(
        std::string(key_cstr),
        val_cstr ? std::string(val_cstr) : std::string()));
  }
}

MetadataInternal::MetadataInternal(StorageInternal* storage)
    : storage_(storage),
      impl_(std::make_unique<FIRStorageMetadataPointer>(
          [[FIRStorageMetadata alloc] init])),
      custom_metadata_(nullptr) {}

MetadataInternal::MetadataInternal(
    StorageInternal* storage, std::unique_ptr<FIRStorageMetadataPointer> impl)
    : storage_(storage), impl_(std::move(impl)), custom_metadata_(nullptr) {}

MetadataInternal::MetadataInternal(const MetadataInternal& metadata)
    : storage_(nullptr), custom_metadata_(nullptr) {
  MutexLock other_lock(metadata.mutex_);
  storage_ = metadata.storage_;
  if (metadata.custom_metadata_) {
    custom_metadata_ =
        std::make_unique<CustomMetadata>(*metadata.custom_metadata_);
  }
  FIRStorageMetadata* copied_impl = CopyObjCMetadataObject(metadata.impl());
  if (copied_impl != nil) {
    impl_ = std::make_unique<FIRStorageMetadataPointer>(copied_impl);
  }
}

MetadataInternal& MetadataInternal::operator=(
    const MetadataInternal& metadata) {
  if (this == &metadata) {
    return *this;
  }
  StorageInternal* other_storage = nullptr;
  FIRStorageMetadata* copied_impl = nil;
  std::unique_ptr<CustomMetadata> copied_custom_metadata;
  {
    MutexLock other_lock(metadata.mutex_);
    other_storage = metadata.storage_;
    copied_impl = CopyObjCMetadataObject(metadata.impl());
    if (metadata.custom_metadata_) {
      copied_custom_metadata =
          std::make_unique<CustomMetadata>(*metadata.custom_metadata_);
    }
  }

  MutexLock lock(mutex_);
  storage_ = other_storage;
  impl_ = copied_impl ? std::make_unique<FIRStorageMetadataPointer>(copied_impl)
                      : nullptr;
  // After impl_ is updated: may reload the existing map from the new impl().
  ReplaceCustomMetadataLocked(copied_custom_metadata.get());
  return *this;
}

MetadataInternal::MetadataInternal(MetadataInternal&& other) noexcept
    : storage_(nullptr), custom_metadata_(nullptr) {
  MutexLock other_lock(other.mutex_);
  storage_ = other.storage_;
  other.storage_ = nullptr;
  impl_ = std::move(other.impl_);
  bucket_ = std::move(other.bucket_);
  cache_control_ = std::move(other.cache_control_);
  content_disposition_ = std::move(other.content_disposition_);
  content_encoding_ = std::move(other.content_encoding_);
  content_language_ = std::move(other.content_language_);
  content_type_ = std::move(other.content_type_);
  name_ = std::move(other.name_);
  path_ = std::move(other.path_);
  md5_hash_ = std::move(other.md5_hash_);
  custom_metadata_ = std::move(other.custom_metadata_);
}

MetadataInternal& MetadataInternal::operator=(
    MetadataInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  StorageInternal* other_storage = nullptr;
  std::unique_ptr<FIRStorageMetadataPointer> other_impl;
  std::string other_bucket;
  std::string other_cache_control;
  std::string other_content_disposition;
  std::string other_content_encoding;
  std::string other_content_language;
  std::string other_content_type;
  std::string other_name;
  std::string other_path;
  std::string other_md5_hash;
  std::unique_ptr<CustomMetadata> other_custom_metadata;
  {
    MutexLock other_lock(other.mutex_);
    other_storage = other.storage_;
    other.storage_ = nullptr;
    other_impl = std::move(other.impl_);
    other_bucket = std::move(other.bucket_);
    other_cache_control = std::move(other.cache_control_);
    other_content_disposition = std::move(other.content_disposition_);
    other_content_encoding = std::move(other.content_encoding_);
    other_content_language = std::move(other.content_language_);
    other_content_type = std::move(other.content_type_);
    other_name = std::move(other.name_);
    other_path = std::move(other.path_);
    other_md5_hash = std::move(other.md5_hash_);
    other_custom_metadata = std::move(other.custom_metadata_);
  }

  {
    MutexLock lock(mutex_);
    storage_ = other_storage;
    impl_ = std::move(other_impl);
    bucket_ = std::move(other_bucket);
    cache_control_ = std::move(other_cache_control);
    content_disposition_ = std::move(other_content_disposition);
    content_encoding_ = std::move(other_content_encoding);
    content_language_ = std::move(other_content_language);
    content_type_ = std::move(other_content_type);
    name_ = std::move(other_name);
    path_ = std::move(other_path);
    md5_hash_ = std::move(other_md5_hash);
    if (other_custom_metadata) {
      if (custom_metadata_) {
        *custom_metadata_ = std::move(*other_custom_metadata);
      } else {
        custom_metadata_ = std::move(other_custom_metadata);
      }
    } else if (custom_metadata_) {
      custom_metadata_->clear();
    }
  }
  return *this;
}

MetadataInternal::~MetadataInternal() = default;

const char* MetadataInternal::bucket() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().bucket : nil, &bucket_);
}

void MetadataInternal::set_cache_control(const char* cache_control) {
  MutexLock lock(mutex_);
  if (impl()) {
    impl().cacheControl = NSStringOrNilFromSetterValue(cache_control);
  }
}

const char* MetadataInternal::cache_control() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().cacheControl : nil, &cache_control_);
}

void MetadataInternal::set_content_disposition(const char* disposition) {
  MutexLock lock(mutex_);
  if (impl()) {
    impl().contentDisposition = NSStringOrNilFromSetterValue(disposition);
  }
}

const char* MetadataInternal::content_disposition() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().contentDisposition : nil,
                           &content_disposition_);
}

void MetadataInternal::set_content_encoding(const char* encoding) {
  MutexLock lock(mutex_);
  if (impl()) {
    impl().contentEncoding = NSStringOrNilFromSetterValue(encoding);
  }
}

const char* MetadataInternal::content_encoding() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().contentEncoding : nil,
                           &content_encoding_);
}

void MetadataInternal::set_content_language(const char* language) {
  MutexLock lock(mutex_);
  if (impl()) {
    impl().contentLanguage = NSStringOrNilFromSetterValue(language);
  }
}

const char* MetadataInternal::content_language() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().contentLanguage : nil,
                           &content_language_);
}

void MetadataInternal::set_content_type(const char* type) {
  MutexLock lock(mutex_);
  if (impl()) {
    impl().contentType = NSStringOrNilFromSetterValue(type);
  }
}

const char* MetadataInternal::content_type() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().contentType : nil, &content_type_);
}

int64_t MetadataInternal::creation_time() const {
  MutexLock lock(mutex_);
  return impl() ? NSDateToMilliseconds(impl().timeCreated) : -1;
}

CustomMetadata* MetadataInternal::custom_metadata() const {
  MutexLock lock(mutex_);
  if (!custom_metadata_) {
    custom_metadata_ = std::make_unique<CustomMetadata>();
    LoadCustomMetadata(impl(), custom_metadata_.get());
  }
  return custom_metadata_.get();
}

void MetadataInternal::ReplaceCustomMetadataLocked(
    CustomMetadata* _Nullable source) {
  if (custom_metadata_) {
    // A pointer to the existing map may have been handed out by
    // custom_metadata(); keep the object and replace only its contents so that
    // pointer stays valid for this object's whole lifetime.
    if (source) {
      *custom_metadata_ = std::move(*source);
    } else {
      LoadCustomMetadata(impl(), custom_metadata_.get());
    }
  } else if (source) {
    custom_metadata_ = std::make_unique<CustomMetadata>(std::move(*source));
  }
  // Otherwise stay lazy: custom_metadata() loads the map from impl() on first
  // use.
}

void MetadataInternal::CommitCustomMetadata() const {
  MutexLock lock(mutex_);
  if (custom_metadata_ && impl()) {
    NSMutableDictionary<NSString*, NSString*>* mutable_metadata =
        [[NSMutableDictionary alloc]
            initWithCapacity:custom_metadata_->size()];
    for (const auto& pair : *custom_metadata_) {
      NSString* key = [NSString stringWithUTF8String:pair.first.c_str()];
      NSString* val = [NSString stringWithUTF8String:pair.second.c_str()];
      if (key != nil && val != nil) {
        [mutable_metadata setObject:val forKey:key];
      }
    }
    impl().customMetadata = mutable_metadata;
  }
}

int64_t MetadataInternal::generation() const {
  MutexLock lock(mutex_);
  return impl() ? impl().generation : -1;
}

int64_t MetadataInternal::metadata_generation() const {
  MutexLock lock(mutex_);
  return impl() ? impl().metageneration : -1;
}

const char* MetadataInternal::name() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().name : nil, &name_);
}

const char* MetadataInternal::path() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().path : nil, &path_);
}

StorageReferenceInternal* MetadataInternal::GetReference() const {
  MutexLock lock(mutex_);
  if (!storage_ || !impl()) {
    return nullptr;
  }
  @autoreleasepool {
    // Hold a strong reference so the UTF8String buffer stays valid while used.
    NSString* path = impl().path;
    if (!path) {
      return nullptr;
    }
    const char* path_cstr = path.UTF8String;
    if (!path_cstr || path_cstr[0] == '\0') {
      return storage_->GetReference();
    }
    return storage_->GetReference(path_cstr);
  }
}

int64_t MetadataInternal::size_bytes() const {
  MutexLock lock(mutex_);
  return impl() ? static_cast<int64_t>(impl().size) : -1;
}

int64_t MetadataInternal::updated_time() const {
  MutexLock lock(mutex_);
  return impl() ? NSDateToMilliseconds(impl().updated) : -1;
}

const char* MetadataInternal::md5_hash() const {
  MutexLock lock(mutex_);
  return NSStringToCString(impl() ? impl().md5Hash : nil, &md5_hash_);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
