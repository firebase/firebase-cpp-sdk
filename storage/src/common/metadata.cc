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

#include "storage/src/include/firebase/storage/metadata.h"

#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/platform.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

// Metadata is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/metadata_android.h"
#include "storage/src/android/storage_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "storage/src/ios/metadata_ios.h"
#include "storage/src/ios/storage_ios.h"
#else
#include "storage/src/desktop/metadata_desktop.h"
#include "storage/src/desktop/storage_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

namespace firebase {
namespace storage {

namespace internal {

class MetadataInternalCommon {
 public:
  static void DeleteInternal(Metadata* metadata) {
    MetadataInternal* internal = metadata->internal_;
    // Since this can trigger a chain of events that deletes the encompassing
    // object, remove the reference to the internal implementation *before*
    // deleting it so that it can't be deleted twice.
    metadata->internal_ = nullptr;
    UnregisterForCleanup(metadata, internal);
    delete internal;
  }

  static void CleanupMetadata(void* metadata_void) {
    DeleteInternal(reinterpret_cast<Metadata*>(metadata_void));
  }

  static internal::StorageInternal* GetStorage(
      internal::MetadataInternal* internal) {
    internal::StorageInternal* storage_internal = nullptr;
    if (internal) storage_internal = internal->storage_internal();
    return storage_internal;
  }

  static void RegisterForCleanup(Metadata* obj,
                                 internal::MetadataInternal* internal) {
    internal::StorageInternal* storage_internal = GetStorage(internal);
    if (storage_internal) {
      storage_internal->cleanup().RegisterObject(obj, CleanupMetadata);
    }
  }

  static void UnregisterForCleanup(Metadata* obj,
                                   internal::MetadataInternal* internal) {
    internal::StorageInternal* storage_internal = GetStorage(internal);
    if (storage_internal) {
      storage_internal->cleanup().UnregisterObject(obj);
    }
  }
};

}  // namespace internal

using internal::MetadataInternal;
using internal::MetadataInternalCommon;

Metadata::Metadata() : internal_(new MetadataInternal(nullptr)) {
  MetadataInternalCommon::RegisterForCleanup(this, internal_);
}

Metadata::Metadata(const Metadata& src) {
  internal_ = src.internal_ ? new MetadataInternal(*src.internal_)
                            : new MetadataInternal(nullptr);
  MetadataInternalCommon::RegisterForCleanup(this, internal_);
}

Metadata::Metadata(MetadataInternal* internal) : internal_(internal) {
  MetadataInternalCommon::RegisterForCleanup(this, internal_);
}

Metadata& Metadata::operator=(const Metadata& src) {
  MetadataInternalCommon::DeleteInternal(this);
  internal_ = src.internal_ ? new MetadataInternal(*src.internal_) : nullptr;
  MetadataInternalCommon::RegisterForCleanup(this, internal_);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
Metadata::Metadata(Metadata&& other) : internal_(other.internal_) {
  MetadataInternalCommon::UnregisterForCleanup(&other, other.internal_);
  other.internal_ = nullptr;
  MetadataInternalCommon::RegisterForCleanup(this, internal_);
}

Metadata& Metadata::operator=(Metadata&& other) {
  MetadataInternalCommon::DeleteInternal(this);
  MetadataInternalCommon::UnregisterForCleanup(&other, other.internal_);
  internal_ = other.internal_;
  other.internal_ = nullptr;
  MetadataInternalCommon::RegisterForCleanup(this, internal_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

Metadata::~Metadata() { MetadataInternalCommon::DeleteInternal(this); }

const char* Metadata::bucket() const {
  return internal_ ? internal_->bucket() : nullptr;
}

void Metadata::set_cache_control(const char* cache_control) {
  if (internal_) {
    internal_->set_cache_control(cache_control);
  }
}

const char* Metadata::cache_control() const {
  return internal_ ? internal_->cache_control() : nullptr;
}

void Metadata::set_content_disposition(const char* disposition) {
  if (internal_) {
    internal_->set_content_disposition(disposition);
  }
}

const char* Metadata::content_disposition() const {
  return internal_ ? internal_->content_disposition() : nullptr;
}

void Metadata::set_content_encoding(const char* encoding) {
  if (internal_) {
    internal_->set_content_encoding(encoding);
  }
}

const char* Metadata::content_encoding() const {
  return internal_ ? internal_->content_encoding() : nullptr;
}

void Metadata::set_content_language(const char* language) {
  if (internal_) {
    internal_->set_content_language(language);
  }
}

const char* Metadata::content_language() const {
  return internal_ ? internal_->content_language() : nullptr;
}

void Metadata::set_content_type(const char* type) {
  if (internal_) {
    internal_->set_content_type(type);
  }
}

const char* Metadata::content_type() const {
  return internal_ ? internal_->content_type() : nullptr;
}

int64_t Metadata::creation_time() const {
  return internal_ ? internal_->creation_time() : 0;
}

std::map<std::string, std::string>* Metadata::custom_metadata() const {
  return internal_ ? internal_->custom_metadata() : nullptr;
}

int64_t Metadata::generation() const {
  return internal_ ? internal_->generation() : 0;
}

int64_t Metadata::metadata_generation() const {
  return internal_ ? internal_->metadata_generation() : 0;
}

const char* Metadata::name() const {
  return internal_ ? internal_->name() : nullptr;
}

const char* Metadata::path() const {
  return internal_ ? internal_->path() : nullptr;
}

StorageReference Metadata::GetReference() const {
  return internal_ ? StorageReference(internal_->GetReference())
                   : StorageReference(nullptr);
}

int64_t Metadata::size_bytes() const {
  return internal_ ? internal_->size_bytes() : -1;
}

int64_t Metadata::updated_time() const {
  return internal_ ? internal_->updated_time() : -1;
}

bool Metadata::is_valid() const { return internal_ != nullptr; }

const char* Metadata::md5_hash() const {
  return internal_ ? internal_->md5_hash() : nullptr;
}

}  // namespace storage
}  // namespace firebase
