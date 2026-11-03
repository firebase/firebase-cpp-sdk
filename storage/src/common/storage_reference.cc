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

#include <memory>
#include <mutex>
#include <unordered_map>

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

// Lifetime protocol between StorageReference handles and the StorageInternal
// that owns their StorageReferenceInternal.
//
// The registration with StorageInternal::cleanup() is keyed by the
// StorageReferenceInternal (which never changes while it is alive), not by
// the user-owned StorageReference handle, and a global registry maps each
// registered internal to the handle that currently owns it. Hence:
//  * Moving a handle only re-points the registry entry under g_handle_mutex;
//    it never touches the CleanupNotifier, so a concurrent CleanupAll() can
//    never observe (or free) a half-moved handle.
//  * The cleanup callback finds the current owner through the registry under
//    g_handle_mutex, so it never dereferences a handle that has been moved
//    from or destroyed.
//  * CleanupNotifier::RegisterObject() never blocks, so it may be called with
//    g_handle_mutex held. UnregisterObject() may block on an in-flight cleanup
//    callback (which takes g_handle_mutex), so it is only ever called while
//    the StorageInternal is provably alive and its CleanupAll() has not
//    started (RunWhileStorageAlive() below), and never with g_handle_mutex
//    held.
//  * While a registry entry exists, its internal's cleanup callback has not
//    run, and that callback needs g_handle_mutex; so while g_handle_mutex is
//    held, a registered internal and its StorageInternal are both alive.
//
// Lock order: g_handle_mutex is never held while acquiring a
// StorageInternal lifetime-token mutex (the token mutex may be held by
// callers, e.g. iOS completion blocks, while user code destroys handles).
namespace {

// Leaked on purpose: handles may be destroyed during static destruction.
std::mutex& HandleMutex() {
  static auto* mutex = new std::mutex();
  return *mutex;
}

// Registered internal -> owning handle (nullptr once the owner released it
// but could not unregister because its StorageInternal is tearing down; the
// cleanup callback then deletes the orphaned internal). Guarded by
// HandleMutex().
std::unordered_map<StorageReferenceInternal*, StorageReference*>& Owners() {
  static auto* owners =
      new std::unordered_map<StorageReferenceInternal*, StorageReference*>();
  return *owners;
}

using StorageToken = std::shared_ptr<StorageInternal::LifetimeToken>;

StorageToken TokenOf(StorageInternal* storage) {
  return storage ? StorageToken(storage->lifetime_token()) : StorageToken();
}

// Runs fn(storage) with `token`'s mutex held if, and only if, the
// StorageInternal is still alive and has not started running its cleanup
// callbacks (every platform's ~StorageInternal() marks the token dead under
// its mutex before CleanupAll()). Returns whether fn ran.
template <typename F>
bool RunWhileStorageAlive(const StorageToken& token, StorageInternal* storage,
                          F&& fn) {
  if (!token || !storage) {
    return false;
  }
#if defined(__ANDROID__)
  std::lock_guard<std::mutex> lock(token->mutex);
  if (!token->alive) {
    return false;
  }
  fn(*storage);
  return true;
#elif TARGET_OS_IPHONE || TARGET_OS_TV
  (void)storage;
  return token->RunWhileAlive(
      [&fn](StorageInternal& alive_storage) { fn(alive_storage); });
#else
  (void)storage;
  std::lock_guard<std::mutex> lock(token->mutex);
  if (token->storage == nullptr) {
    return false;
  }
  fn(*token->storage);
  return true;
#endif  // defined(__ANDROID__), TARGET_OS_IPHONE || TARGET_OS_TV
}

}  // namespace

class StorageReferenceInternalCommon {
 public:
  // Gives `handle` (which must not own an internal) ownership of the newly
  // created, unregistered `internal`, registering it for cleanup.
  static void Attach(StorageReference* handle,
                     StorageReferenceInternal* internal) {
    std::lock_guard<std::mutex> lock(HandleMutex());
    AttachLocked(handle, internal);
  }

  // Makes `handle` (which must not own an internal) own a copy of `source`'s
  // internal.
  static void CopyFrom(StorageReference* handle,
                       const StorageReference& source) {
    std::lock_guard<std::mutex> lock(HandleMutex());
    // source.internal_ (and its StorageInternal) cannot be freed by a cleanup
    // callback while HandleMutex() is held (see above).
    if (source.internal_ == nullptr) {
      return;
    }
    AttachLocked(handle, new StorageReferenceInternal(*source.internal_));
  }

  // Transfers ownership of `source`'s internal to `handle` (which must not
  // own an internal). Never touches the CleanupNotifier.
  static void MoveFrom(StorageReference* handle, StorageReference* source) {
    std::lock_guard<std::mutex> lock(HandleMutex());
    StorageReferenceInternal* internal = source->internal_;
    source->internal_ = nullptr;
    handle->internal_ = internal;
    if (internal != nullptr) {
      auto it = Owners().find(internal);
      if (it != Owners().end()) {
        it->second = handle;
      }
    }
  }

  // Releases and deletes the internal owned by `handle`, if any.
  static void Release(StorageReference* handle) {
    StorageReferenceInternal* internal = nullptr;
    StorageInternal* storage = nullptr;
    StorageToken token;
    {
      std::lock_guard<std::mutex> lock(HandleMutex());
      internal = handle->internal_;
      handle->internal_ = nullptr;
      if (internal == nullptr) {
        return;
      }
      auto it = Owners().find(internal);
      if (it != Owners().end()) {
        // Orphan the entry: if the StorageInternal is already tearing down,
        // its cleanup callback deletes `internal`.
        it->second = nullptr;
        storage = internal->storage_internal();
        token = TokenOf(storage);
      }
    }
    if (storage != nullptr) {
      // While the StorageInternal is alive no cleanup callback is in flight,
      // so UnregisterObject() does not block. If it is tearing down, its
      // cleanup callback owns the orphaned `internal`: do not touch it.
      const bool unregistered = RunWhileStorageAlive(
          token, storage, [internal](StorageInternal& alive_storage) {
            alive_storage.cleanup().UnregisterObject(internal);
          });
      if (!unregistered) {
        return;
      }
      std::lock_guard<std::mutex> lock(HandleMutex());
      Owners().erase(internal);
    }
    delete internal;
  }

 private:
  static void AttachLocked(StorageReference* handle,
                           StorageReferenceInternal* internal) {
    handle->internal_ = internal;
    StorageInternal* storage =
        internal != nullptr ? internal->storage_internal() : nullptr;
    if (storage == nullptr) {
      return;
    }
    Owners()[internal] = handle;
    // Never blocks; if CleanupAll() is running it will pick this up.
    storage->cleanup().RegisterObject(internal, CleanupStorageReference);
  }

  // Cleanup callback, run by StorageInternal::cleanup().CleanupAll().
  static void CleanupStorageReference(void* internal_void) {
    auto* internal = static_cast<StorageReferenceInternal*>(internal_void);
    {
      std::lock_guard<std::mutex> lock(HandleMutex());
      auto it = Owners().find(internal);
      if (it == Owners().end()) {
        return;  // Already released and unregistered by its owner.
      }
      StorageReference* owner = it->second;
      Owners().erase(it);
      if (owner != nullptr) {
        owner->internal_ = nullptr;
      }
    }
    delete internal;
  }
};

}  // namespace internal

using internal::StorageReferenceInternal;
using internal::StorageReferenceInternalCommon;

StorageReference::StorageReference(StorageReferenceInternal* internal)
    : internal_(nullptr) {
  StorageReferenceInternalCommon::Attach(this, internal);
}

StorageReference::StorageReference(const StorageReference& other)
    : internal_(nullptr) {
  StorageReferenceInternalCommon::CopyFrom(this, other);
}

StorageReference& StorageReference::operator=(const StorageReference& other) {
  if (this == &other) {
    return *this;
  }
  StorageReference copy(other);
  StorageReferenceInternalCommon::Release(this);
  StorageReferenceInternalCommon::MoveFrom(this, &copy);
  return *this;
}

StorageReference::StorageReference(StorageReference&& other) noexcept
    : internal_(nullptr) {
  StorageReferenceInternalCommon::MoveFrom(this, &other);
}

StorageReference& StorageReference::operator=(
    StorageReference&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  StorageReferenceInternalCommon::Release(this);
  StorageReferenceInternalCommon::MoveFrom(this, &other);
  return *this;
}

StorageReference::~StorageReference() {
  StorageReferenceInternalCommon::Release(this);
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
