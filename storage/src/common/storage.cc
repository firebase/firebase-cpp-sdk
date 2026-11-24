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

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "app/src/app_state.h"
#include "app/src/assert.h"
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
    storage, { return ::firebase::kInitResultSuccess; }, false);

namespace firebase {
namespace storage {

DEFINE_FIREBASE_VERSION_STRING(FirebaseStorage);

namespace {
constexpr int kStorageServiceTag = 0;
}  // namespace

Storage::Storage(std::shared_ptr<internal::StorageInternal> internal)
    : internal_(std::move(internal)) {}

Storage Storage::GetInstance(const ::firebase::App& app,
                             InitResult* init_result_out) {
  return GetInstance(app, nullptr, init_result_out);
}

Storage Storage::GetInstance(const ::firebase::App& app, const char* url,
                             InitResult* init_result_out) {
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Storage();
  }

  // URL we use as the per-App service key.
  // If a null/empty URL is given, we look up the App's default storage bucket.
  std::string url_idx;
  if (url != nullptr && strlen(url) > 0) {
    url_idx = url;
  } else {
    const char* bucket = app.options().storage_bucket();
    url_idx = std::string(internal::kCloudStorageScheme) +
              (bucket ? bucket : "");
  }

  // Validate storage URL.
  static const char kObjectName[] = "Storage";
  std::string path;
  if (!internal::UriToComponents(url_idx, kObjectName, nullptr, &path)) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Storage();
  }
  if (!path.empty()) {
    LogError(
        "Unable to create %s from URL %s. "
        "URL should specify a bucket without a path.",
        kObjectName, url_idx.c_str());
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Storage();
  }

  auto internal = state->GetOrCreateService<internal::StorageInternal>(
      &kStorageServiceTag, url_idx,
      [&app, url](::firebase::internal::AppState& app_state)
          -> std::shared_ptr<internal::StorageInternal> {
        auto inst =
            std::make_shared<internal::StorageInternal>(app_state, app, url);
        if (!inst->initialized()) {
          return nullptr;
        }
        return inst;
      });

  if (!internal) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Storage();
  }

  if (init_result_out != nullptr) {
    *init_result_out = kInitResultSuccess;
  }
  return Storage(std::move(internal));
}

::firebase::App Storage::app() const {
  return internal_ ? internal_->app() : ::firebase::App();
}

std::string Storage::url() const {
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
  if (!internal_ || url == nullptr) return StorageReference(nullptr);

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

void Storage::UseEmulator(const char* host, int port) {
  if (internal_) internal_->UseEmulator(host, port);
}

}  // namespace storage
}  // namespace firebase
