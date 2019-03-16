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

#include "app/src/assert.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

// StorageReference is defined in these 3 files, one implementation for each OS.
#if defined(__ANDROID__)
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"
#elif TARGET_OS_IPHONE
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/storage_reference_ios.h"
#else
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#endif  // defined(__ANDROID__), TARGET_OS_IPHONE

namespace firebase {
namespace storage {

static void AssertMetadataIsValid(const Metadata& metadata) {
  FIREBASE_ASSERT_MESSAGE(metadata.is_valid(),
                          "The specified Metadata is not valid.");
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
  StorageReferenceInternalCommon::DeleteInternal(this);
  internal_ = other.internal_ ? new StorageReferenceInternal(*other.internal_)
                              : nullptr;
  StorageReferenceInternalCommon::RegisterForCleanup(this, internal_);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
StorageReference::StorageReference(StorageReference&& other) {
  StorageReferenceInternalCommon::UnregisterForCleanup(&other, other.internal_);
  internal_ = other.internal_;
  other.internal_ = nullptr;
  StorageReferenceInternalCommon::RegisterForCleanup(this, internal_);
}

StorageReference& StorageReference::operator=(StorageReference&& other) {
  StorageReferenceInternalCommon::DeleteInternal(this);
  StorageReferenceInternalCommon::UnregisterForCleanup(&other, other.internal_);
  internal_ = other.internal_;
  other.internal_ = nullptr;
  StorageReferenceInternalCommon::RegisterForCleanup(this, internal_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

StorageReference::~StorageReference() {
  StorageReferenceInternalCommon::DeleteInternal(this);
}

Storage* StorageReference::storage() {
  return internal_ ? internal_->storage() : nullptr;
}

StorageReference StorageReference::Child(const char* path) const {
  return internal_ ? StorageReference(internal_->Child(path))
                   : StorageReference(nullptr);
}

Future<void> StorageReference::Delete() {
  return internal_ ? internal_->Delete() : Future<void>();
}

Future<void> StorageReference::DeleteLastResult() {
  return internal_ ? internal_->DeleteLastResult() : Future<void>();
}

std::string StorageReference::bucket() {
  return internal_ ? internal_->bucket() : std::string();
}

std::string StorageReference::full_path() {
  return internal_ ? internal_->full_path() : std::string();
}

Future<size_t> StorageReference::GetFile(const char* path, Listener* listener,
                                         Controller* controller_out) {
  return internal_ ? internal_->GetFile(path, listener, controller_out)
                   : Future<size_t>();
}

Future<size_t> StorageReference::GetFileLastResult() {
  return internal_ ? internal_->GetFileLastResult() : Future<size_t>();
}

Future<size_t> StorageReference::GetBytes(void* buffer, size_t buffer_size,
                                          Listener* listener,
                                          Controller* controller_out) {
  return internal_ ? internal_->GetBytes(buffer, buffer_size, listener,
                                         controller_out)
                   : Future<size_t>();
}

Future<size_t> StorageReference::GetBytesLastResult() {
  return internal_ ? internal_->GetBytesLastResult() : Future<size_t>();
}

Future<std::string> StorageReference::GetDownloadUrl() {
  return internal_ ? internal_->GetDownloadUrl() : Future<std::string>();
}

Future<std::string> StorageReference::GetDownloadUrlLastResult() {
  return internal_ ? internal_->GetDownloadUrlLastResult()
                   : Future<std::string>();
}

Future<Metadata> StorageReference::GetMetadata() {
  return internal_ ? internal_->GetMetadata() : Future<Metadata>();
}

Future<Metadata> StorageReference::GetMetadataLastResult() {
  return internal_ ? internal_->GetMetadataLastResult() : Future<Metadata>();
}

Future<Metadata> StorageReference::UpdateMetadata(const Metadata& metadata) {
  AssertMetadataIsValid(metadata);
  return internal_ ? internal_->UpdateMetadata(&metadata) : Future<Metadata>();
}

Future<Metadata> StorageReference::UpdateMetadataLastResult() {
  return internal_ ? internal_->UpdateMetadataLastResult() : Future<Metadata>();
}

std::string StorageReference::name() {
  return internal_ ? internal_->name() : std::string();
}

StorageReference StorageReference::GetParent() {
  return internal_ ? StorageReference(internal_->GetParent())
                   : StorageReference(nullptr);
}

Future<Metadata> StorageReference::PutBytes(const void* buffer,
                                            size_t buffer_size,
                                            Listener* listener,
                                            Controller* controller_out) {
  return internal_ ? internal_->PutBytes(buffer, buffer_size, listener,
                                         controller_out)
                   : Future<Metadata>();
}

Future<Metadata> StorageReference::PutBytes(const void* buffer,
                                            size_t buffer_size,
                                            const Metadata& metadata,
                                            Listener* listener,
                                            Controller* controller_out) {
  AssertMetadataIsValid(metadata);
  return internal_ ? internal_->PutBytes(buffer, buffer_size, &metadata,
                                         listener, controller_out)
                   : Future<Metadata>();
}

Future<Metadata> StorageReference::PutBytesLastResult() {
  return internal_ ? internal_->PutBytesLastResult() : Future<Metadata>();
}

Future<Metadata> StorageReference::PutFile(const char* path, Listener* listener,
                                           Controller* controller_out) {
  return internal_ ? internal_->PutFile(path, listener, controller_out)
                   : Future<Metadata>();
}

Future<Metadata> StorageReference::PutFile(const char* path,
                                           const Metadata& metadata,
                                           Listener* listener,
                                           Controller* controller_out) {
  AssertMetadataIsValid(metadata);
  return internal_
             ? internal_->PutFile(path, &metadata, listener, controller_out)
             : Future<Metadata>();
}

Future<Metadata> StorageReference::PutFileLastResult() {
  return internal_ ? internal_->PutFileLastResult() : Future<Metadata>();
}

bool StorageReference::is_valid() const { return internal_ != nullptr; }

}  // namespace storage
}  // namespace firebase
