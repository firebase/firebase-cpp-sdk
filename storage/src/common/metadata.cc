// Copyright 2016 Google LLC
#include "storage/src/include/firebase/storage/metadata.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/platform.h"
#include "storage/src/common/metadata_internal.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/storage_reference_ios.h"
#else
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#endif

namespace firebase::storage {
namespace internal {
namespace {

using StorageToken = std::shared_ptr<StorageInternal::LifetimeToken>;
struct OwnerEntry {
  Metadata* owner = nullptr;
  StorageInternal* storage = nullptr;
  StorageToken token;
};

std::mutex& HandleMutex() {
  static auto* m = new std::mutex();
  return *m;
}

std::unordered_map<MetadataInternal*, OwnerEntry>& Owners() {
  static auto* o = new std::unordered_map<MetadataInternal*, OwnerEntry>();
  return *o;
}

template <typename F>
bool RunWhileStorageAlive(const StorageToken& token, StorageInternal* storage,
                          F&& fn) {
  if (!token || !storage) return false;
#if FIREBASE_PLATFORM_ANDROID
  std::lock_guard<std::mutex> lock(token->mutex);
  if (!token->alive) return false;
  fn(*storage);
  return true;
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
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

class MetadataInternalCommon {
 public:
  static void Attach(Metadata* handle, MetadataInternal* internal) {
    if (!internal) {
      std::lock_guard<std::mutex> lock(HandleMutex());
      handle->internal_ = nullptr;
      return;
    }
    StorageReferenceInternal* ref = internal->reference_internal();
    StorageInternal* storage = ref ? ref->storage_internal() : nullptr;
    AttachWithStorage(handle, internal, storage,
                      storage ? storage->lifetime_token() : StorageToken());
  }

  static void CopyFrom(Metadata* handle, const Metadata& source) {
    MetadataInternal* internal = nullptr;
    StorageInternal* storage = nullptr;
    StorageToken token;
    {
      std::lock_guard<std::mutex> lock(HandleMutex());
      if (!source.internal_) {
        handle->internal_ = nullptr;
        return;
      }
      internal = new MetadataInternal(*source.internal_);
      auto it = Owners().find(source.internal_);
      if (it != Owners().end()) {
        storage = it->second.storage;
        token = it->second.token;
      }
    }
    AttachWithStorage(handle, internal, storage, token);
  }

  static void MoveFrom(Metadata* handle, Metadata* source) noexcept {
    std::lock_guard<std::mutex> lock(HandleMutex());
    MetadataInternal* internal = source->internal_;
    source->internal_ = nullptr;
    handle->internal_ = internal;
    if (internal) {
      auto it = Owners().find(internal);
      if (it != Owners().end()) it->second.owner = handle;
    }
  }

  static void Release(Metadata* handle) {
    MetadataInternal* internal = nullptr;
    StorageInternal* storage = nullptr;
    StorageToken token;
    {
      std::lock_guard<std::mutex> lock(HandleMutex());
      internal = handle->internal_;
      handle->internal_ = nullptr;
      if (!internal) return;
      auto it = Owners().find(internal);
      if (it != Owners().end()) {
        it->second.owner = nullptr;
        storage = it->second.storage;
        token = it->second.token;
      }
    }
    if (storage) {
      RunWhileStorageAlive(token, storage, [&](StorageInternal& alive) {
        alive.cleanup().UnregisterObject(internal);
      });
      std::lock_guard<std::mutex> lock(HandleMutex());
      if (Owners().erase(internal) == 0) return;
    }
    delete internal;
  }

 private:
  static void AttachWithStorage(Metadata* handle, MetadataInternal* internal,
                                StorageInternal* storage,
                                const StorageToken& token) {
    if (!storage) {
      std::lock_guard<std::mutex> lock(HandleMutex());
      handle->internal_ = internal;
      return;
    }
    {
      std::lock_guard<std::mutex> lock(HandleMutex());
      handle->internal_ = internal;
      Owners()[internal] = OwnerEntry{handle, storage, token};
    }
    if (!RunWhileStorageAlive(token, storage, [internal](StorageInternal& s) {
          s.cleanup().RegisterObject(internal, CleanupMetadata);
        })) {
      {
        std::lock_guard<std::mutex> lock(HandleMutex());
        auto it = Owners().find(internal);
        if (it == Owners().end()) return;
        if (it->second.owner) it->second.owner->internal_ = nullptr;
        Owners().erase(it);
      }
      delete internal;
    }
  }

  static void CleanupMetadata(void* internal_void) {
    auto* internal = static_cast<MetadataInternal*>(internal_void);
    {
      std::lock_guard<std::mutex> lock(HandleMutex());
      auto it = Owners().find(internal);
      if (it == Owners().end()) return;
      if (it->second.owner) it->second.owner->internal_ = nullptr;
      Owners().erase(it);
    }
    delete internal;
  }
};

}  // namespace internal

using internal::MetadataInternal;
using internal::MetadataInternalCommon;

Metadata::Metadata() : internal_(nullptr) {
  MetadataInternalCommon::Attach(this, new MetadataInternal(nullptr));
}
Metadata::Metadata(const Metadata& src) : internal_(nullptr) {
  MetadataInternalCommon::CopyFrom(this, src);
}
Metadata::Metadata(MetadataInternal* internal) : internal_(nullptr) {
  MetadataInternalCommon::Attach(this, internal);
}
Metadata& Metadata::operator=(const Metadata& src) {
  if (this != &src) {
    Metadata copy(src);
    MetadataInternalCommon::Release(this);
    MetadataInternalCommon::MoveFrom(this, &copy);
  }
  return *this;
}
Metadata::Metadata(Metadata&& other) noexcept : internal_(nullptr) {
  MetadataInternalCommon::MoveFrom(this, &other);
}
Metadata& Metadata::operator=(Metadata&& other) noexcept {
  if (this != &other) {
    MetadataInternalCommon::Release(this);
    MetadataInternalCommon::MoveFrom(this, &other);
  }
  return *this;
}
Metadata::~Metadata() { MetadataInternalCommon::Release(this); }

const char* Metadata::bucket() const {
  return internal_ ? internal_->bucket() : nullptr;
}
void Metadata::set_cache_control(const char* v) {
  if (internal_) internal_->set_cache_control(v);
}
const char* Metadata::cache_control() const {
  return internal_ ? internal_->cache_control() : nullptr;
}
void Metadata::set_content_disposition(const char* v) {
  if (internal_) internal_->set_content_disposition(v);
}
const char* Metadata::content_disposition() const {
  return internal_ ? internal_->content_disposition() : nullptr;
}
void Metadata::set_content_encoding(const char* v) {
  if (internal_) internal_->set_content_encoding(v);
}
const char* Metadata::content_encoding() const {
  return internal_ ? internal_->content_encoding() : nullptr;
}
void Metadata::set_content_language(const char* v) {
  if (internal_) internal_->set_content_language(v);
}
const char* Metadata::content_language() const {
  return internal_ ? internal_->content_language() : nullptr;
}
void Metadata::set_content_type(const char* v) {
  if (internal_) internal_->set_content_type(v);
}
const char* Metadata::content_type() const {
  return internal_ ? internal_->content_type() : nullptr;
}
int64_t Metadata::creation_time() const {
  return internal_ ? internal_->creation_time() : -1;
}
std::map<std::string, std::string>* Metadata::custom_metadata() const {
  return internal_ ? internal_->custom_metadata() : nullptr;
}
int64_t Metadata::generation() const {
  return internal_ ? internal_->generation() : -1;
}
int64_t Metadata::metadata_generation() const {
  return internal_ ? internal_->metadata_generation() : -1;
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

static bool NullableStrEqual(const char* a, const char* b) {
  return std::strcmp(a ? a : "", b ? b : "") == 0;
}

bool Metadata::operator==(const Metadata& other) const {
  if (is_valid() != other.is_valid()) return false;
  if (!is_valid()) return true;
  if (!NullableStrEqual(bucket(), other.bucket()) ||
      !NullableStrEqual(cache_control(), other.cache_control()) ||
      !NullableStrEqual(content_disposition(), other.content_disposition()) ||
      !NullableStrEqual(content_encoding(), other.content_encoding()) ||
      !NullableStrEqual(content_language(), other.content_language()) ||
      !NullableStrEqual(content_type(), other.content_type()) ||
      !NullableStrEqual(name(), other.name()) ||
      !NullableStrEqual(path(), other.path()) ||
      !NullableStrEqual(md5_hash(), other.md5_hash()) ||
      creation_time() != other.creation_time() ||
      updated_time() != other.updated_time() ||
      generation() != other.generation() ||
      metadata_generation() != other.metadata_generation() ||
      size_bytes() != other.size_bytes()) {
    return false;
  }
  const auto* cm_a = custom_metadata();
  const auto* cm_b = other.custom_metadata();
  if (!cm_a && !cm_b) return true;
  if (!cm_a) return cm_b->empty();
  if (!cm_b) return cm_a->empty();
  return *cm_a == *cm_b;
}

}  // namespace firebase::storage
