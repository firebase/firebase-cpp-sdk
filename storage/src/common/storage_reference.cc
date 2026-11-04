// Copyright 2016 Google LLC
#include "storage/src/include/firebase/storage/storage_reference.h"

#include <memory>
#include <mutex>
#include <unordered_map>

#include "app/src/log.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/controller.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#if defined(__ANDROID__)
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"
#elif TARGET_OS_IPHONE || TARGET_OS_TV
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/storage_reference_ios.h"
#else
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#endif

namespace firebase::storage {

static bool CheckMetadataIsValid(const Metadata& metadata) {
  if (!metadata.is_valid()) {
    LogError("The specified Metadata is not valid.");
    return false;
  }
  return true;
}

namespace internal {
namespace {

std::mutex& HandleMutex() {
  static auto* m = new std::mutex();
  return *m;
}

std::unordered_map<StorageReferenceInternal*, StorageReference*>& Owners() {
  static auto* o =
      new std::unordered_map<StorageReferenceInternal*, StorageReference*>();
  return *o;
}

using StorageToken = std::shared_ptr<StorageInternal::LifetimeToken>;

template <typename F>
bool RunWhileStorageAlive(const StorageToken& token, StorageInternal* storage,
                          F&& fn) {
  if (!token || !storage) return false;
#if defined(__ANDROID__)
  std::lock_guard<std::mutex> lock(token->mutex);
  if (!token->alive) return false;
  fn(*storage);
  return true;
#elif TARGET_OS_IPHONE || TARGET_OS_TV
  (void)storage;
  return token->RunWhileAlive([&fn](StorageInternal& alive) { fn(alive); });
#else
  (void)storage;
  std::lock_guard<std::mutex> lock(token->mutex);
  if (!token->storage) return false;
  fn(*token->storage);
  return true;
#endif
}

}  // namespace

class StorageReferenceInternalCommon {
 public:
  static void Attach(StorageReference* handle,
                     StorageReferenceInternal* internal) {
    std::lock_guard<std::mutex> lock(HandleMutex());
    AttachLocked(handle, internal);
  }

  static void CopyFrom(StorageReference* handle,
                       const StorageReference& source) {
    std::lock_guard<std::mutex> lock(HandleMutex());
    if (!source.internal_) return;
    AttachLocked(handle, new StorageReferenceInternal(*source.internal_));
  }

  static void MoveFrom(StorageReference* handle, StorageReference* source) {
    std::lock_guard<std::mutex> lock(HandleMutex());
    StorageReferenceInternal* internal = source->internal_;
    source->internal_ = nullptr;
    handle->internal_ = internal;
    if (internal) {
      auto it = Owners().find(internal);
      if (it != Owners().end()) it->second = handle;
    }
  }

  static void Release(StorageReference* handle) {
    StorageReferenceInternal* internal = nullptr;
    StorageInternal* storage = nullptr;
    StorageToken token;
    {
      std::lock_guard<std::mutex> lock(HandleMutex());
      internal = handle->internal_;
      handle->internal_ = nullptr;
      if (!internal) return;
      auto it = Owners().find(internal);
      if (it != Owners().end()) {
        it->second = nullptr;
        storage = internal->storage_internal();
        if (storage) token = storage->lifetime_token();
      }
    }
    if (storage) {
      RunWhileStorageAlive(token, storage, [&](StorageInternal& alive) {
        alive.cleanup().UnregisterObject(internal);
      });
      std::lock_guard<std::mutex> lock(HandleMutex());
      Owners().erase(internal);
    }
    delete internal;
  }

 private:
  static void AttachLocked(StorageReference* handle,
                           StorageReferenceInternal* internal) {
    handle->internal_ = internal;
    StorageInternal* storage = internal ? internal->storage_internal() : nullptr;
    if (!storage) return;
    Owners()[internal] = handle;
    storage->cleanup().RegisterObject(internal, CleanupStorageReference);
  }

  static void CleanupStorageReference(void* internal_void) {
    auto* internal = static_cast<StorageReferenceInternal*>(internal_void);
    {
      std::lock_guard<std::mutex> lock(HandleMutex());
      auto it = Owners().find(internal);
      if (it == Owners().end()) return;
      if (it->second) it->second->internal_ = nullptr;
      Owners().erase(it);
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
  if (this != &other) {
    StorageReference copy(other);
    StorageReferenceInternalCommon::Release(this);
    StorageReferenceInternalCommon::MoveFrom(this, &copy);
  }
  return *this;
}
StorageReference::StorageReference(StorageReference&& other) noexcept
    : internal_(nullptr) {
  StorageReferenceInternalCommon::MoveFrom(this, &other);
}
StorageReference& StorageReference::operator=(
    StorageReference&& other) noexcept {
  if (this != &other) {
    StorageReferenceInternalCommon::Release(this);
    StorageReferenceInternalCommon::MoveFrom(this, &other);
  }
  return *this;
}
StorageReference::~StorageReference() {
  StorageReferenceInternalCommon::Release(this);
}

Storage* StorageReference::storage() const {
  return internal_ ? internal_->storage() : nullptr;
}
StorageReference StorageReference::Child(const char* path) const {
  return (internal_ && path) ? StorageReference(internal_->Child(path))
                             : StorageReference(nullptr);
}
std::string StorageReference::bucket() const {
  return internal_ ? internal_->bucket() : std::string();
}
std::string StorageReference::full_path() const {
  return internal_ ? internal_->full_path() : std::string();
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
  const size_t first = path.find_first_not_of('/');
  return first == std::string::npos ? std::string() : path.substr(first);
}

template <typename T>
Future<T> LastResultOf(const StorageReferenceInternal* internal,
                       internal::StorageReferenceFn fn) {
  return internal ? static_cast<const Future<T>&>(
                        internal->future()->LastResult(fn))
                  : Future<T>();
}
}  // namespace

StorageReference StorageReference::GetRoot() const {
  Storage* s = is_valid() ? storage() : nullptr;
  if (!s) return StorageReference(nullptr);
  std::string b = bucket();
  return b.empty() ? s->GetReference()
                   : s->GetReferenceFromUrl(("gs://" + b).c_str());
}

bool StorageReference::is_root() const {
  return is_valid() && NormalizeStoragePath(full_path()).empty();
}

Future<void> StorageReference::Delete() {
  if (!internal_) return Future<void>();
  auto h = internal_->future()->SafeAlloc<void>(internal::kStorageReferenceFnDelete);
  internal_->DeleteImpl(h, internal_->future());
  return Future<void>(internal_->future(), h.get());
}
Future<void> StorageReference::DeleteLastResult() const {
  return LastResultOf<void>(internal_, internal::kStorageReferenceFnDelete);
}

Future<size_t> StorageReference::GetFile(const char* path, Listener* listener,
                                         Controller* controller_out) {
  if (!internal_) {
    if (controller_out) *controller_out = Controller();
    return Future<size_t>();
  }
  auto h = internal_->future()->SafeAlloc<size_t>(internal::kStorageReferenceFnGetFile);
  if (!path || path[0] == '\0') {
    if (controller_out) *controller_out = Controller();
    internal_->future()->Complete(h, kErrorInvalidArgument,
                                  "File path cannot be null or empty.");
    return Future<size_t>(internal_->future(), h.get());
  }
  internal_->GetFileImpl(path, listener, controller_out, h, internal_->future());
  return Future<size_t>(internal_->future(), h.get());
}
Future<size_t> StorageReference::GetFileLastResult() const {
  return LastResultOf<size_t>(internal_, internal::kStorageReferenceFnGetFile);
}

Future<size_t> StorageReference::GetBytes(void* buffer, size_t buffer_size,
                                          Listener* listener,
                                          Controller* controller_out) {
  if (!internal_) {
    if (controller_out) *controller_out = Controller();
    return Future<size_t>();
  }
  auto h = internal_->future()->SafeAlloc<size_t>(internal::kStorageReferenceFnGetBytes);
  internal_->GetBytesImpl(buffer, buffer_size, listener, controller_out, h,
                          internal_->future());
  return Future<size_t>(internal_->future(), h.get());
}
Future<size_t> StorageReference::GetBytesLastResult() const {
  return LastResultOf<size_t>(internal_, internal::kStorageReferenceFnGetBytes);
}

Future<std::string> StorageReference::GetDownloadUrl() {
  if (!internal_) return Future<std::string>();
  auto h = internal_->future()->SafeAlloc<std::string>(
      internal::kStorageReferenceFnGetDownloadUrl);
  internal_->GetDownloadUrlImpl(h, internal_->future());
  return Future<std::string>(internal_->future(), h.get());
}
Future<std::string> StorageReference::GetDownloadUrlLastResult() const {
  return LastResultOf<std::string>(internal_,
                                   internal::kStorageReferenceFnGetDownloadUrl);
}

Future<Metadata> StorageReference::GetMetadata() {
  if (!internal_) return Future<Metadata>();
  auto h = internal_->future()->SafeAlloc<Metadata>(
      internal::kStorageReferenceFnGetMetadata);
  internal_->GetMetadataImpl(h, internal_->future());
  return Future<Metadata>(internal_->future(), h.get());
}
Future<Metadata> StorageReference::GetMetadataLastResult() const {
  return LastResultOf<Metadata>(internal_,
                                internal::kStorageReferenceFnGetMetadata);
}

Future<Metadata> StorageReference::UpdateMetadata(const Metadata& metadata) {
  if (!internal_ || !CheckMetadataIsValid(metadata)) return Future<Metadata>();
  auto h = internal_->future()->SafeAlloc<Metadata>(
      internal::kStorageReferenceFnUpdateMetadata);
  internal_->UpdateMetadataImpl(&metadata, h, internal_->future());
  return Future<Metadata>(internal_->future(), h.get());
}
Future<Metadata> StorageReference::UpdateMetadataLastResult() const {
  return LastResultOf<Metadata>(internal_,
                                internal::kStorageReferenceFnUpdateMetadata);
}

Future<Metadata> StorageReference::PutBytes(const void* buffer,
                                            size_t buffer_size,
                                            Listener* listener,
                                            Controller* controller_out) {
  if (!internal_) {
    if (controller_out) *controller_out = Controller();
    return Future<Metadata>();
  }
  auto h = internal_->future()->SafeAlloc<Metadata>(
      internal::kStorageReferenceFnPutBytes);
  internal_->PutBytesImpl(buffer, buffer_size, listener, controller_out, h,
                          internal_->future());
  return Future<Metadata>(internal_->future(), h.get());
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
  auto h = internal_->future()->SafeAlloc<Metadata>(
      internal::kStorageReferenceFnPutBytes);
  internal_->PutBytesImpl(buffer, buffer_size, &metadata, listener,
                          controller_out, h, internal_->future());
  return Future<Metadata>(internal_->future(), h.get());
}
Future<Metadata> StorageReference::PutBytesLastResult() const {
  return LastResultOf<Metadata>(internal_,
                                internal::kStorageReferenceFnPutBytes);
}

Future<Metadata> StorageReference::PutFile(const char* path, Listener* listener,
                                           Controller* controller_out) {
  if (!internal_) {
    if (controller_out) *controller_out = Controller();
    return Future<Metadata>();
  }
  auto h = internal_->future()->SafeAlloc<Metadata>(
      internal::kStorageReferenceFnPutFile);
  if (!path || path[0] == '\0') {
    if (controller_out) *controller_out = Controller();
    internal_->future()->Complete(h, kErrorInvalidArgument,
                                  "File path cannot be null or empty.");
    return Future<Metadata>(internal_->future(), h.get());
  }
  internal_->PutFileImpl(path, listener, controller_out, h, internal_->future());
  return Future<Metadata>(internal_->future(), h.get());
}

Future<Metadata> StorageReference::PutFile(const char* path,
                                           const Metadata& metadata,
                                           Listener* listener,
                                           Controller* controller_out) {
  if (!internal_ || !CheckMetadataIsValid(metadata)) {
    if (controller_out) *controller_out = Controller();
    return Future<Metadata>();
  }
  auto h = internal_->future()->SafeAlloc<Metadata>(
      internal::kStorageReferenceFnPutFile);
  if (!path || path[0] == '\0') {
    if (controller_out) *controller_out = Controller();
    internal_->future()->Complete(h, kErrorInvalidArgument,
                                  "File path cannot be null or empty.");
    return Future<Metadata>(internal_->future(), h.get());
  }
  internal_->PutFileImpl(path, &metadata, listener, controller_out, h,
                         internal_->future());
  return Future<Metadata>(internal_->future(), h.get());
}
Future<Metadata> StorageReference::PutFileLastResult() const {
  return LastResultOf<Metadata>(internal_,
                                internal::kStorageReferenceFnPutFile);
}

Future<StorageListResult> StorageReference::List(int max_results_per_page,
                                                 const char* page_token) {
  if (!internal_) return Future<StorageListResult>();
  auto h = internal_->future()->SafeAlloc<StorageListResult>(
      internal::kStorageReferenceFnList);
  if (max_results_per_page <= 0 || max_results_per_page > 1000) {
    internal_->future()->Complete(
        h, kErrorInvalidArgument,
        "max_results_per_page must be > 0 and <= 1000.");
    return Future<StorageListResult>(internal_->future(), h.get());
  }
  internal_->ListImpl(max_results_per_page, page_token, h, internal_->future());
  return Future<StorageListResult>(internal_->future(), h.get());
}
Future<StorageListResult> StorageReference::ListLastResult() const {
  return LastResultOf<StorageListResult>(internal_,
                                         internal::kStorageReferenceFnList);
}

Future<StorageListResult> StorageReference::ListAll() {
  if (!internal_) return Future<StorageListResult>();
  auto h = internal_->future()->SafeAlloc<StorageListResult>(
      internal::kStorageReferenceFnListAll);
  internal_->ListAllImpl(h, internal_->future());
  return Future<StorageListResult>(internal_->future(), h.get());
}
Future<StorageListResult> StorageReference::ListAllLastResult() const {
  return LastResultOf<StorageListResult>(internal_,
                                         internal::kStorageReferenceFnListAll);
}

bool StorageReference::is_valid() const { return internal_ != nullptr; }

std::string StorageReference::ToString() const {
  if (!is_valid()) return std::string();
  std::string b = bucket();
  std::string p = NormalizeStoragePath(full_path());
  return p.empty() ? ("gs://" + b) : ("gs://" + b + "/" + p);
}

bool StorageReference::operator==(const StorageReference& other) const {
  if (is_valid() != other.is_valid()) return false;
  if (!is_valid()) return true;
  return storage() == other.storage() && bucket() == other.bucket() &&
         NormalizeStoragePath(full_path()) ==
             NormalizeStoragePath(other.full_path());
}

#if defined(FIREBASE_STORAGE_BUILDING_SDK)
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
#endif

}  // namespace firebase::storage
