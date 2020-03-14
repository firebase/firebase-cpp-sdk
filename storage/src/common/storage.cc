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

#include <assert.h>
#include <string.h>

#include <map>
#include <string>

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/util.h"
#include "storage/src/common/storage_uri_parser.h"

// QueryInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/storage_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "storage/src/ios/storage_ios.h"
#else
#include "storage/src/desktop/storage_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    storage,
    {
      FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(*app);
      return ::firebase::kInitResultSuccess;
    },
    {
        // Nothing to tear down.
    });

namespace firebase {
namespace storage {

DEFINE_FIREBASE_VERSION_STRING(FirebaseStorage);

Mutex g_storages_lock;  // NOLINT
static std::map<std::pair<App*, std::string>, Storage*>* g_storages = nullptr;

Storage* Storage::GetInstance(::firebase::App* app,
                              InitResult* init_result_out) {
  return GetInstance(app, nullptr, init_result_out);
}

Storage* Storage::GetInstance(::firebase::App* app, const char* url,
                              InitResult* init_result_out) {
  MutexLock lock(g_storages_lock);
  if (!g_storages) {
    g_storages = new std::map<std::pair<App*, std::string>, Storage*>();
  }
  // URL we use for our global index of Storage instances.
  // If a null URL is given, we look up the App's default storage bucket.
  std::string url_idx;
  if (url != nullptr && strlen(url) > 0) {
    url_idx = url;
  } else {
    url_idx = std::string(internal::kCloudStorageScheme) +
              app->options().storage_bucket();
  }

  // Validate storage URL.
  static const char kObjectName[] = "Storage";
  std::string path;
  if (!internal::UriToComponents(url_idx, kObjectName, nullptr, &path)) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }
  if (!path.empty()) {
    LogError(
        "Unable to create %s from URL %s. "
        "URL should specify a bucket without a path.",
        kObjectName, url_idx.c_str());
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }

  std::map<std::pair<App*, std::string>, Storage*>::iterator it =
      g_storages->find(std::make_pair(app, url_idx));
  if (it != g_storages->end()) {
    if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
    return it->second;
  }
  FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(*app, init_result_out);

  Storage* storage = new Storage(app, url);
  if (!storage->internal_->initialized()) {
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    delete storage;
    return nullptr;
  }
  g_storages->insert(std::make_pair(std::make_pair(app, url_idx), storage));
  if (init_result_out) *init_result_out = kInitResultSuccess;
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
          "Storage object 0x%08x should be deleted before the App 0x%08x "
          "it depends upon.",
          static_cast<int>(reinterpret_cast<intptr_t>(storage)),
          static_cast<int>(reinterpret_cast<intptr_t>(storage->app())));
      storage->DeleteInternal();
    });
  }
}

void Storage::DeleteInternal() {
  MutexLock lock(g_storages_lock);
  if (!internal_) return;

  CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(app());
  assert(app_notifier);
  app_notifier->UnregisterObject(this);

  // Force cleanup to happen first.
  internal_->cleanup().CleanupAll();
  // If a Storage is explicitly deleted, remove it from our cache.
  std::string url_idx = url().empty()
                            ? std::string(internal::kCloudStorageScheme) +
                                  app()->options().storage_bucket()
                            : url();
  g_storages->erase(std::make_pair(app(), url_idx));
  delete internal_;
  internal_ = nullptr;
  // If it's the last one, delete the map.
  if (g_storages->empty()) {
    delete g_storages;
    g_storages = nullptr;
  }
}

Storage::~Storage() { DeleteInternal(); }

::firebase::App* Storage::app() {
  return internal_ ? internal_->app() : nullptr;
}

std::string Storage::url() {
  return internal_ ? internal_->url() : std::string();
}

StorageReference Storage::GetReference() const {
  return internal_ ? StorageReference(internal_->GetReference())
                   : StorageReference(nullptr);
}

StorageReference Storage::GetReference(const char* path) const {
  return internal_ ? StorageReference(internal_->GetReference(path))
                   : StorageReference(nullptr);
}

StorageReference Storage::GetReferenceFromUrl(const char* url) const {
  if (!internal_) return StorageReference(nullptr);

  static const char kObjectName[] = "StorageReference";
  // Extract components from this storage object's URL.
  std::string this_bucket = const_cast<Storage*>(this)->GetReference().bucket();
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

double Storage::max_download_retry_time() {
  return internal_ ? internal_->max_download_retry_time() : 0;
}

void Storage::set_max_download_retry_time(double max_transfer_retry_seconds) {
  if (internal_)
    internal_->set_max_download_retry_time(max_transfer_retry_seconds);
}

double Storage::max_upload_retry_time() {
  return internal_ ? internal_->max_upload_retry_time() : 0;
}

void Storage::set_max_upload_retry_time(double max_transfer_retry_seconds) {
  if (internal_)
    internal_->set_max_upload_retry_time(max_transfer_retry_seconds);
}

double Storage::max_operation_retry_time() {
  return internal_ ? internal_->max_operation_retry_time() : 0;
}

void Storage::set_max_operation_retry_time(double max_transfer_retry_seconds) {
  if (internal_)
    return internal_->set_max_operation_retry_time(max_transfer_retry_seconds);
}

}  // namespace storage
}  // namespace firebase
