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

#include "storage/src/include/firebase/storage.h"

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "storage/src/common/storage_uri_parser.h"

// StorageInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/storage_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/storage_ios.h"
#else
#include "storage/src/desktop/storage_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    storage, { return ::firebase::kInitResultSuccess; },
    {
        // Nothing to tear down.
    },
    false);

namespace firebase {
namespace storage {

DEFINE_FIREBASE_VERSION_STRING(FirebaseStorage);

Mutex g_storages_lock;  // NOLINT
static std::map<std::pair<App*, std::string>, Storage*>* g_storages = nullptr;
static std::atomic<LogLevel> g_storage_log_level{kLogLevelWarning};

Storage* Storage::GetInstance() {
  return GetInstance(App::GetInstance(), nullptr, nullptr);
}

Storage* Storage::GetInstance(const std::string& url,
                              InitResult* init_result_out) {
  return GetInstance(App::GetInstance(), url.c_str(), init_result_out);
}

Storage* Storage::GetInstance(::firebase::App* app,
                              InitResult* init_result_out) {
  return GetInstance(app, nullptr, init_result_out);
}

Storage* Storage::GetInstance(::firebase::App* app, const char* url,
                              InitResult* init_result_out) {
  static const char kObjectName[] = "Storage";
  if (!app) {
    LogError("Unable to create %s: firebase::App cannot be null.", kObjectName);
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }

  // URL we use for our global index of Storage instances.
  // If a null/empty URL is given, we look up the App's default storage bucket.
  std::string raw_url;
  if (url != nullptr && url[0] != '\0') {
    raw_url = url;
  } else {
    const char* default_bucket = app->options().storage_bucket();
    if (!default_bucket || default_bucket[0] == '\0') {
      LogError(
          "Unable to create %s: no default storage_bucket is configured in "
          "AppOptions and no bucket URL was specified.",
          kObjectName);
      if (init_result_out != nullptr) {
        *init_result_out = kInitResultFailedMissingDependency;
      }
      return nullptr;
    }
    raw_url = default_bucket;
    if (raw_url.compare(0, std::strlen(internal::kCloudStorageScheme),
                        internal::kCloudStorageScheme) != 0 &&
        raw_url.compare(0, 7, "http://") != 0 &&
        raw_url.compare(0, 8, "https://") != 0) {
      raw_url = std::string(internal::kCloudStorageScheme) + raw_url;
    }
  }

  // Validate storage URL and normalize bucket key.
  std::string bucket;
  std::string path;
  if (!internal::UriToComponents(raw_url, kObjectName, &bucket, &path)) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }
  if (!path.empty()) {
    LogError(
        "Unable to create %s from URL %s. "
        "URL should specify a bucket without a path.",
        kObjectName, raw_url.c_str());
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }
  std::string url_idx =
      bucket.empty() ? std::string()
                     : std::string(internal::kCloudStorageScheme) + bucket;
  const auto key = std::make_pair(app, url_idx);

  {
    MutexLock lock(g_storages_lock);
    if (!g_storages) {
      g_storages = new std::map<std::pair<App*, std::string>, Storage*>();
    }
    auto it = g_storages->find(key);
    if (it != g_storages->end()) {
      if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
      return it->second;
    }
  }

  Storage* storage =
      new Storage(app, url_idx.empty() ? nullptr : url_idx.c_str());
  if (!storage->internal_ || !storage->internal_->initialized()) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    delete storage;
    return nullptr;
  }

  Storage* existing_storage = nullptr;
  {
    MutexLock lock(g_storages_lock);
    if (!g_storages) {
      g_storages = new std::map<std::pair<App*, std::string>, Storage*>();
    }
    auto it = g_storages->find(key);
    if (it != g_storages->end()) {
      existing_storage = it->second;
    } else {
      g_storages->insert(std::make_pair(key, storage));
    }
  }

  if (existing_storage != nullptr) {
    delete storage;
    storage = existing_storage;
  }
  if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
  return storage;
}

Storage::Storage(::firebase::App* app, const char* url) {
  internal_ = new internal::StorageInternal(app, url);

  if (internal_->initialized()) {
    CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(app);
    assert(app_notifier);
    app_notifier->RegisterObject(this, [](void* object) {
      Storage* storage = reinterpret_cast<Storage*>(object);
      LogWarning(
          "Storage object %p should be deleted before the App %p "
          "it depends upon.",
          static_cast<const void*>(storage),
          static_cast<const void*>(storage->app()));
      storage->DeleteInternal();
    });
  }
}

void Storage::DeleteInternal() {
  internal::StorageInternal* internal = nullptr;
  {
    MutexLock lock(g_storages_lock);
    if (!internal_) return;
    internal = internal_;
    internal_ = nullptr;

    if (g_storages) {
      for (auto it = g_storages->begin(); it != g_storages->end();) {
        if (it->second == this) {
          it = g_storages->erase(it);
        } else {
          ++it;
        }
      }
      if (g_storages->empty()) {
        delete g_storages;
        g_storages = nullptr;
      }
    }
  }

  // An instance whose initialization failed was never registered with the
  // App's CleanupNotifier, and app() may be null.
  if (!internal->initialized()) {
    delete internal;
    return;
  }

  CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(internal->app());
  if (app_notifier) {
    app_notifier->UnregisterObject(this);
  }

  // ~StorageInternal() marks its lifetime token dead and then runs
  // cleanup().CleanupAll() itself, on every platform. Do not call CleanupAll()
  // here first: running cleanup callbacks while the StorageInternal still
  // reports itself alive would let them race with (and deadlock against)
  // owners that unregister under the lifetime token.
  delete internal;
}

Storage::~Storage() { DeleteInternal(); }

::firebase::App* Storage::app() const {
  return internal_ ? internal_->app() : nullptr;
}

std::string Storage::url() const {
  return internal_ ? internal_->url() : std::string();
}

StorageReference Storage::GetReference() const {
  return internal_ ? StorageReference(internal_->GetReference())
                   : StorageReference(nullptr);
}

StorageReference Storage::GetReference(const char* path) const {
  if (!internal_) return StorageReference(nullptr);
  if (!path) {
    LogError("Storage::GetReference() called with null path.");
    return StorageReference(nullptr);
  }
  return StorageReference(internal_->GetReference(path));
}

StorageReference Storage::GetReferenceFromUrl(const char* url) const {
  if (!internal_) return StorageReference(nullptr);
  if (!url) {
    LogError("Storage::GetReferenceFromUrl() called with null url.");
    return StorageReference(nullptr);
  }

  static const char kObjectName[] = "StorageReference";
  // Extract components from this storage object's URL.
  std::string this_bucket = GetReference().bucket();
  // Make sure the specified URL is valid.
  std::string bucket;
  bool valid = internal::UriToComponents(std::string(url), kObjectName, &bucket,
                                         nullptr);
  if (valid) {
    if (!this_bucket.empty() && bucket != this_bucket) {
      LogError(
          "Unable to create %s from URL %s. "
          "URL specifies a different bucket (%s) than this instance (%s)",
          kObjectName, url, bucket.c_str(), this_bucket.c_str());
      valid = false;
    }
  }
  return StorageReference(valid ? internal_->GetReferenceFromUrl(url)
                                : nullptr);
}

double Storage::max_download_retry_time() const {
  return internal_ ? internal_->max_download_retry_time() : 0;
}

void Storage::set_max_download_retry_time(double max_transfer_retry_seconds) {
  if (!std::isfinite(max_transfer_retry_seconds) ||
      max_transfer_retry_seconds < 0.0) {
    LogError("max_download_retry_time must be a finite non-negative value.");
    return;
  }
  if (internal_) {
    internal_->set_max_download_retry_time(max_transfer_retry_seconds);
  }
}

double Storage::max_upload_retry_time() const {
  return internal_ ? internal_->max_upload_retry_time() : 0;
}

void Storage::set_max_upload_retry_time(double max_transfer_retry_seconds) {
  if (!std::isfinite(max_transfer_retry_seconds) ||
      max_transfer_retry_seconds < 0.0) {
    LogError("max_upload_retry_time must be a finite non-negative value.");
    return;
  }
  if (internal_) {
    internal_->set_max_upload_retry_time(max_transfer_retry_seconds);
  }
}

double Storage::max_operation_retry_time() const {
  return internal_ ? internal_->max_operation_retry_time() : 0;
}

void Storage::set_max_operation_retry_time(double max_operation_retry_seconds) {
  if (!std::isfinite(max_operation_retry_seconds) ||
      max_operation_retry_seconds < 0.0) {
    LogError("max_operation_retry_time must be a finite non-negative value.");
    return;
  }
  if (internal_) {
    internal_->set_max_operation_retry_time(max_operation_retry_seconds);
  }
}

void Storage::UseEmulator(const char* host, int port) {
  if (!internal_ || host == nullptr || host[0] == '\0' || port <= 0) return;
  internal_->UseEmulator(host, port);
}

void Storage::set_log_level(LogLevel log_level) {
  g_storage_log_level.store(log_level, std::memory_order_relaxed);
}

LogLevel Storage::log_level() const {
  return g_storage_log_level.load(std::memory_order_relaxed);
}

#if defined(FIREBASE_STORAGE_BUILDING_SDK)
// ABI-compatibility non-const overloads (see storage.h).
::firebase::App* Storage::app() {
  return static_cast<const Storage*>(this)->app();
}
std::string Storage::url() { return static_cast<const Storage*>(this)->url(); }
double Storage::max_download_retry_time() {
  return static_cast<const Storage*>(this)->max_download_retry_time();
}
double Storage::max_upload_retry_time() {
  return static_cast<const Storage*>(this)->max_upload_retry_time();
}
double Storage::max_operation_retry_time() {
  return static_cast<const Storage*>(this)->max_operation_retry_time();
}
#endif  // defined(FIREBASE_STORAGE_BUILDING_SDK)

}  // namespace storage
}  // namespace firebase
