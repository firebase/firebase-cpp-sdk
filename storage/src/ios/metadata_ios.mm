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

#import "FIRStorageMetadata.h"

#import <Foundation/Foundation.h>

#include "storage/src/ios/metadata_ios.h"

extern NSString* const kFIRStorageMetadataSize;

namespace firebase {
namespace storage {
namespace internal {

// Create a copy of a FIRStorageMetadata.
static FIRStorageMetadata* CopyObjCMetadataObject(const FIRStorageMetadata* src) {
  return [src copy];
}

// Convert a NSString to UTF8 string returning a reference to the buffer in
// the std::string.
static const char* NSStringToCString(NSString *nsstring_value,
                                     std::string *string_backing_store) {
  string_backing_store->clear();
  const char* utf8_string = nsstring_value.UTF8String;
  if (utf8_string) {
    *string_backing_store = utf8_string;
  }
  return string_backing_store->c_str();
}

// Convert NSDate to milliseconds since the epoch.
static int64_t NSDateToMilliseconds(NSDate *date) {
  double seconds_since_the_epoch = date.timeIntervalSince1970;
  int64_t milliseconds_since_the_epoch = static_cast<int64_t>(seconds_since_the_epoch * 1000.0);
  return milliseconds_since_the_epoch;
}

MetadataInternal::MetadataInternal() : storage_(nullptr), custom_metadata_(nullptr) {
  impl_.reset(new FIRStorageMetadataPointer([[FIRStorageMetadata alloc] init]));
}

MetadataInternal::MetadataInternal(StorageInternal* storage)
    : storage_(storage), custom_metadata_(nullptr) {
  impl_.reset(new FIRStorageMetadataPointer([[FIRStorageMetadata alloc] init]));
}

MetadataInternal::MetadataInternal(StorageInternal* storage,
                                   UniquePtr<FIRStorageMetadataPointer> impl)
    : storage_(storage), impl_(std::move(impl)), custom_metadata_(nullptr) {}

MetadataInternal::MetadataInternal(const MetadataInternal& metadata)
    : storage_(metadata.storage_),
      custom_metadata_(metadata.custom_metadata_
                           ? new std::map<std::string, std::string>(*metadata.custom_metadata_)
                           : nullptr) {
  impl_.reset(new FIRStorageMetadataPointer(CopyObjCMetadataObject(metadata.impl())));
}

MetadataInternal& MetadataInternal::operator=(const MetadataInternal& metadata) {
  storage_ = metadata.storage_;
  impl_.reset(new FIRStorageMetadataPointer(CopyObjCMetadataObject(metadata.impl())));

  if (custom_metadata_) delete custom_metadata_;
  custom_metadata_ = metadata.custom_metadata_ ?
      new std::map<std::string, std::string>(*metadata.custom_metadata_) : nullptr;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
MetadataInternal::MetadataInternal(MetadataInternal&& other) {
  storage_ = other.storage_;
  other.storage_ = nullptr;
  impl_ = std::move(other.impl_);
  other.impl_ = MakeUnique<FIRStorageMetadataPointer>([[FIRStorageMetadata alloc] init]);
  custom_metadata_ = std::move(other.custom_metadata_);
}

MetadataInternal& MetadataInternal::operator=(MetadataInternal&& other) {
  storage_ = other.storage_;
  other.storage_ = nullptr;
  impl_ = other.impl_;
  other.impl_ = MakeUnique<FIRStorageMetadataPointer>([[FIRStorageMetadata alloc] init]);
  if (custom_metadata_) delete custom_metadata_;
  custom_metadata_ = std::move(other.custom_metadata_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

MetadataInternal::~MetadataInternal() {
  if (custom_metadata_) {
    delete custom_metadata_;
  }
}

const char* MetadataInternal::bucket() { return NSStringToCString(impl().bucket, &bucket_); }

void MetadataInternal::set_cache_control(const char* cache_control) {
  impl().cacheControl = @(cache_control);
}

const char* MetadataInternal::cache_control() {
  return NSStringToCString(impl().cacheControl, &cache_control_);
}

void MetadataInternal::set_content_disposition(const char* disposition) {
  impl().contentDisposition = @(disposition);
}

const char* MetadataInternal::content_disposition() {
  return NSStringToCString(impl().contentDisposition, &content_disposition_);
}

void MetadataInternal::set_content_encoding(const char* encoding) {
  impl().contentEncoding = @(encoding);
}

const char* MetadataInternal::content_encoding() {
  return NSStringToCString(impl().contentEncoding, &content_encoding_);
}

void MetadataInternal::set_content_language(const char* language) {
  impl().contentLanguage = @(language);
}

const char* MetadataInternal::content_language() {
  return NSStringToCString(impl().contentLanguage, &content_language_);
}

void MetadataInternal::set_content_type(const char* type) { impl().contentType = @(type); }

const char* MetadataInternal::content_type() {
  return NSStringToCString(impl().contentType, &content_type_);
}

int64_t MetadataInternal::creation_time() { return NSDateToMilliseconds(impl().timeCreated); }

std::map<std::string, std::string>* MetadataInternal::custom_metadata() {
  if (custom_metadata_ == nullptr) {
    custom_metadata_ = new std::map<std::string, std::string>();
    if (impl()) {
      for (NSString* key in impl().customMetadata) {
        NSString* value = [impl().customMetadata objectForKey:key];
        custom_metadata_->insert(
            std::make_pair(std::string(key.UTF8String),
                           value ? std::string(value.UTF8String) : std::string()));
      }
    }
  }
  return custom_metadata_;
}

void MetadataInternal::CommitCustomMetadata() const {
  if (custom_metadata_) {
    NSMutableDictionary *mutableMetadata =
        [[NSMutableDictionary alloc] initWithCapacity:custom_metadata_->size()];
    for (auto& pair : *custom_metadata_) {
      [mutableMetadata setObject:@(pair.second.c_str())
                          forKey:@(pair.first.c_str())];
    }
    impl().customMetadata = mutableMetadata;
  }
}

int64_t MetadataInternal::generation() { return impl().generation; }

int64_t MetadataInternal::metadata_generation() { return impl().metageneration; }

const char* MetadataInternal::name() { return NSStringToCString(impl().name, &name_); }

const char* MetadataInternal::path() { return NSStringToCString(impl().path, &path_); }

StorageReferenceInternal* MetadataInternal::GetReference() {
  if (storage_ && impl()) {
    return new StorageReferenceInternal(
        storage_, MakeUnique<FIRStorageReferencePointer>(impl().storageReference));
  } else {
    return nullptr;
  }
}

int64_t MetadataInternal::size_bytes() { return static_cast<int64_t>(impl().size); }

int64_t MetadataInternal::updated_time() { return NSDateToMilliseconds(impl().updated); }

const char* MetadataInternal::md5_hash() { return NSStringToCString(impl().md5Hash, &md5_hash_); }

}  // namespace internal
}  // namespace storage
}  // namespace firebase
