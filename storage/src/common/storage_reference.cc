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

#include "storage/src/include/firebase/storage/storage_reference.h"

#include "app/src/log.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/controller.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

// StorageReference is defined in these 3 files, one implementation for each OS.
#if defined(__ANDROID__)
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"
#elif TARGET_OS_IPHONE || TARGET_OS_TV
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/storage_reference_ios.h"
#else
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#endif  // defined(__ANDROID__), TARGET_OS_IPHONE

namespace firebase {
namespace storage {

static bool CheckMetadataIsValid(const Metadata& metadata) {
  if (!metadata.is_valid()) {
    LogError("The specified Metadata is not valid.");
    return false;
  }
  return true;
}

namespace internal {

class StorageReferenceInternalCommon {
 public:
  static void DeleteInternal(StorageReference* storage_reference) {
    StorageReferenceInternal* internal = storage_reference->internal_;
    // Since this can trigger a chain of events that deletes the encompassing
    // object, remove the reference to the internal implementation *before*
    // deleting it so that it can't be deleted twice.
    storage_reference->internal_ = nullptr;
    UnregisterForCleanup(storage_reference, internal);
    delete internal;
  }

  static void CleanupStorageReference(void* storage_reference_void) {
    DeleteInternal(reinterpret_cast<StorageReference*>(storage_reference_void));
  }

  static void RegisterForCleanup(StorageReference* obj,
                                 StorageReferenceInternal* internal) {
    if (internal && internal->storage_internal()) {
      internal->storage_internal()->cleanup().RegisterObject(
          obj, CleanupStorageReference);
    }
  }

  static void UnregisterForCleanup(StorageReference* obj,
                                   StorageReferenceInternal* internal) {
    if (internal && internal->storage_internal()) {
      internal->storage_internal()->cleanup().UnregisterObject(obj);
    }
  }
};

}  // namespace internal

using internal::StorageReferenceInternal;
using internal::StorageReferenceInternalCommon;

StorageReference::StorageReference(StorageReferenceInternal* internal)
    : internal_(internal) {
  StorageReferenceInternalCommon::RegisterForCleanup(this, internal_);
}

StorageReference::StorageReference(const StorageReference& other)
    : internal_(other.internal_ ? new StorageReferenceInternal(*other.internal_)
                                : nullptr) {
  StorageReferenceInternalCommon::RegisterForCleanup(this, internal_);
}

StorageReference& StorageReference::operator=(const StorageReference& other) {
  if (this == &other) {
    return *this;
  }
  StorageReferenceInternal* new_internal =
      other.internal_ ? new StorageReferenceInternal(*other.internal_)
                      : nullptr;
  StorageReferenceInternalCommon::DeleteInternal(this);
  internal_ = new_internal;
  StorageReferenceInternalCommon::RegisterForCleanup(this, internal_);
  return *this;
}

StorageReference::StorageReference(StorageReference&& other) noexcept
    : internal_(other.internal_) {
  StorageReferenceInternalCommon::UnregisterForCleanup(&other, other.internal_);
  other.internal_ = nullptr;
  StorageReferenceInternalCommon::RegisterForCleanup(this, internal_);
}

StorageReference& StorageReference::operator=(
    StorageReference&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  StorageReferenceInternalCommon::DeleteInternal(this);
  StorageReferenceInternalCommon::UnregisterForCleanup(&other, other.internal_);
  internal_ = other.internal_;
  other.internal_ = nullptr;
  StorageReferenceInternalCommon::RegisterForCleanup(this, internal_);
  return *this;
}

StorageReference::~StorageReference() {
  StorageReferenceInternalCommon::DeleteInternal(this);
}

Storage* StorageReference::storage() const {
  return internal_ ? internal_->storage() : nullptr;
}

StorageReference StorageReference::Child(const char* path) const {
  if (!internal_ || !path) {
    return StorageReference(nullptr);
  }
  return StorageReference(internal_->Child(path));
}

Future<void> StorageReference::Delete() {
  return internal_ ? internal_->Delete() : Future<void>();
}

Future<void> StorageReference::DeleteLastResult() const {
  return internal_ ? internal_->DeleteLastResult() : Future<void>();
}

std::string StorageReference::bucket() const {
  return internal_ ? internal_->bucket() : std::string();
}

std::string StorageReference::full_path() const {
  return internal_ ? internal_->full_path() : std::string();
}

Future<size_t> StorageReference::GetFile(const char* path, Listener* listener,
                                         Controller* controller_out) {
  if (!internal_) {
    if (controller_out) *controller_out = Controller();
    return Future<size_t>();
  }
  // A null/empty `path` is validated by every platform implementation,
  // which completes the returned Future with kErrorInvalidArgument.
  return internal_->GetFile(path, listener, controller_out);
}

Future<size_t> StorageReference::GetFileLastResult() const {
  return internal_ ? internal_->GetFileLastResult() : Future<size_t>();
}

Future<size_t> StorageReference::GetBytes(void* buffer, size_t buffer_size,
                                          Listener* listener,
                                          Controller* controller_out) {
  if (!internal_) {
    if (controller_out) *controller_out = Controller();
    return Future<size_t>();
  }
  return internal_->GetBytes(buffer, buffer_size, listener, controller_out);
}

Future<size_t> StorageReference::GetBytesLastResult() const {
  return internal_ ? internal_->GetBytesLastResult() : Future<size_t>();
}

Future<std::string> StorageReference::GetDownloadUrl() {
  return internal_ ? internal_->GetDownloadUrl() : Future<std::string>();
}

Future<std::string> StorageReference::GetDownloadUrlLastResult() const {
  return internal_ ? internal_->GetDownloadUrlLastResult()
                   : Future<std::string>();
}

Future<Metadata> StorageReference::GetMetadata() {
  return internal_ ? internal_->GetMetadata() : Future<Metadata>();
}

Future<Metadata> StorageReference::GetMetadataLastResult() const {
  return internal_ ? internal_->GetMetadataLastResult() : Future<Metadata>();
}

Future<Metadata> StorageReference::UpdateMetadata(const Metadata& metadata) {
  if (!internal_ || !CheckMetadataIsValid(metadata)) {
    return Future<Metadata>();
  }
  return internal_->UpdateMetadata(&metadata);
}

Future<Metadata> StorageReference::UpdateMetadataLastResult() const {
  return internal_ ? internal_->UpdateMetadataLastResult() : Future<Metadata>();
}

std::string StorageReference::name() const {
  return internal_ ? internal_->name() : std::string();
}

StorageReference StorageReference::GetParent() const {
  return internal_ ? StorageReference(internal_->GetParent())
                   : StorageReference(nullptr);
}

namespace {

std::string NormalizeStoragePath(const std::string& path) {
  const size_t first_non_slash = path.find_first_not_of('/');
  if (first_non_slash == std::string::npos) {
    return std::string();
  }
  return path.substr(first_non_slash);
}

}  // namespace

StorageReference StorageReference::GetRoot() const {
  if (!is_valid()) {
    return StorageReference(nullptr);
  }
  Storage* s = storage();
  if (!s) {
    return StorageReference(nullptr);
  }
  std::string b = bucket();
  if (!b.empty()) {
    return s->GetReferenceFromUrl(("gs://" + b).c_str());
  }
  return s->GetReference();
}

bool StorageReference::is_root() const {
  if (!is_valid()) return false;
  return NormalizeStoragePath(full_path()).empty();
}

Future<Metadata> StorageReference::PutBytes(const void* buffer,
                                            size_t buffer_size,
                                            Listener* listener,
                                            Controller* controller_out) {
  if (!internal_) {
    if (controller_out) *controller_out = Controller();
    return Future<Metadata>();
  }
  return internal_->PutBytes(buffer, buffer_size, listener, controller_out);
}

Future<Metadata> StorageReference::PutBytes(const void* buffer,
                                            size_t buffer_size,
                                            const Metadata& metadata,
                                            Listener* listener,
                                            Controller* controller_out) {
  if (!internal_ || !CheckMetadataIsValid(metadata)) {
    if (controller_out) *controller_out = Controller();
    return Future<Metadata>();
  }
  return internal_->PutBytes(buffer, buffer_size, &metadata, listener,
                             controller_out);
}

Future<Metadata> StorageReference::PutBytesLastResult() const {
  return internal_ ? internal_->PutBytesLastResult() : Future<Metadata>();
}

Future<Metadata> StorageReference::PutFile(const char* path, Listener* listener,
                                           Controller* controller_out) {
  if (!internal_) {
    if (controller_out) *controller_out = Controller();
    return Future<Metadata>();
  }
  // A null/empty `path` is validated by every platform implementation,
  // which completes the returned Future with kErrorInvalidArgument.
  return internal_->PutFile(path, listener, controller_out);
}

Future<Metadata> StorageReference::PutFile(const char* path,
                                           const Metadata& metadata,
                                           Listener* listener,
                                           Controller* controller_out) {
  if (!internal_ || !CheckMetadataIsValid(metadata)) {
    if (controller_out) *controller_out = Controller();
    return Future<Metadata>();
  }
  // A null/empty `path` is validated by every platform implementation,
  // which completes the returned Future with kErrorInvalidArgument.
  return internal_->PutFile(path, &metadata, listener, controller_out);
}

Future<Metadata> StorageReference::PutFileLastResult() const {
  return internal_ ? internal_->PutFileLastResult() : Future<Metadata>();
}

Future<StorageListResult> StorageReference::List(int max_results_per_page,
                                                 const char* page_token) {
  return internal_ ? internal_->List(max_results_per_page, page_token)
                   : Future<StorageListResult>();
}

Future<StorageListResult> StorageReference::ListLastResult() const {
  return internal_ ? internal_->ListLastResult() : Future<StorageListResult>();
}

Future<StorageListResult> StorageReference::ListAll() {
  return internal_ ? internal_->ListAll() : Future<StorageListResult>();
}

Future<StorageListResult> StorageReference::ListAllLastResult() const {
  return internal_ ? internal_->ListAllLastResult()
                   : Future<StorageListResult>();
}

bool StorageReference::is_valid() const { return internal_ != nullptr; }

std::string StorageReference::ToString() const {
  if (!is_valid()) {
    return std::string();
  }
  std::string b = bucket();
  std::string p = NormalizeStoragePath(full_path());
  if (p.empty()) {
    return "gs://" + b;
  }
  return "gs://" + b + "/" + p;
}

bool StorageReference::operator==(const StorageReference& other) const {
  if (is_valid() != other.is_valid()) {
    return false;
  }
  if (!is_valid()) {
    return true;
  }
  return storage() == other.storage() && bucket() == other.bucket() &&
         NormalizeStoragePath(full_path()) ==
             NormalizeStoragePath(other.full_path());
}

#if defined(FIREBASE_STORAGE_BUILDING_SDK)
// ABI-compatibility non-const overloads (see storage_reference.h).
Storage* StorageReference::storage() {
  return static_cast<const StorageReference*>(this)->storage();
}
Future<void> StorageReference::DeleteLastResult() {
  return static_cast<const StorageReference*>(this)->DeleteLastResult();
}
std::string StorageReference::bucket() {
  return static_cast<const StorageReference*>(this)->bucket();
}
std::string StorageReference::full_path() {
  return static_cast<const StorageReference*>(this)->full_path();
}
Future<size_t> StorageReference::GetFileLastResult() {
  return static_cast<const StorageReference*>(this)->GetFileLastResult();
}
Future<size_t> StorageReference::GetBytesLastResult() {
  return static_cast<const StorageReference*>(this)->GetBytesLastResult();
}
Future<std::string> StorageReference::GetDownloadUrlLastResult() {
  return static_cast<const StorageReference*>(this)->GetDownloadUrlLastResult();
}
Future<Metadata> StorageReference::GetMetadataLastResult() {
  return static_cast<const StorageReference*>(this)->GetMetadataLastResult();
}
Future<Metadata> StorageReference::UpdateMetadataLastResult() {
  return static_cast<const StorageReference*>(this)->UpdateMetadataLastResult();
}
Future<StorageListResult> StorageReference::ListLastResult() {
  return static_cast<const StorageReference*>(this)->ListLastResult();
}
std::string StorageReference::name() {
  return static_cast<const StorageReference*>(this)->name();
}
StorageReference StorageReference::GetParent() {
  return static_cast<const StorageReference*>(this)->GetParent();
}
Future<Metadata> StorageReference::PutBytesLastResult() {
  return static_cast<const StorageReference*>(this)->PutBytesLastResult();
}
Future<Metadata> StorageReference::PutFileLastResult() {
  return static_cast<const StorageReference*>(this)->PutFileLastResult();
}
#endif  // defined(FIREBASE_STORAGE_BUILDING_SDK)

}  // namespace storage
}  // namespace firebase
