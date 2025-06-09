// Copyright 2025 Google LLC
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

#include "storage/src/include/firebase/storage/list_result.h"

#include <assert.h>

// #include "storage/src/common/list_result_internal_common.h" // Removed
#include "app/src/cleanup_notifier.h"      // For CleanupNotifier
#include "app/src/include/firebase/app.h"  // For App, LogDebug
#include "app/src/util.h"                  // For LogDebug
#include "storage/src/common/storage_internal.h"  // Required for StorageInternal, CleanupNotifier
#include "storage/src/common/storage_reference_internal.h"  // For StorageReference constructor from internal

// Platform specific ListResultInternal definitions
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/list_result_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "storage/src/ios/list_result_ios.h"
#elif FIREBASE_PLATFORM_DESKTOP
#include "storage/src/desktop/list_result_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_DESKTOP

namespace firebase {
namespace storage {

using internal::ListResultInternal;
// using internal::ListResultInternalCommon; // Removed

// Global function to be called by CleanupNotifier
// This function is responsible for cleaning up the internal state of a
// ListResult object when the App is being shut down.
static void GlobalCleanupListResult(void* list_result_void) {
  if (list_result_void) {
    ListResult* list_result = static_cast<ListResult*>(list_result_void);
    // This method will delete internal_ and set it to nullptr.
    list_result->ClearInternalForCleanup();
  }
}

ListResult::ListResult() : internal_(nullptr) {}

ListResult::ListResult(internal::ListResultInternal* internal)
    : internal_(internal) {
  if (internal_ && internal_->storage_internal() &&
      internal_->storage_internal()->app_valid()) {
    internal_->storage_internal()->cleanup().RegisterObject(
        this, GlobalCleanupListResult);
  }
}

ListResult::ListResult(const ListResult& other) : internal_(nullptr) {
  if (other.internal_) {
    internal_ = new internal::ListResultInternal(*other.internal_);
  }
  if (internal_ && internal_->storage_internal() &&
      internal_->storage_internal()->app_valid()) {
    internal_->storage_internal()->cleanup().RegisterObject(
        this, GlobalCleanupListResult);
  }
}

ListResult& ListResult::operator=(const ListResult& other) {
  if (this == &other) {
    return *this;
  }

  // Unregister and delete current internal object
  if (internal_) {
    if (internal_->storage_internal() &&
        internal_->storage_internal()->app_valid()) {
      internal_->storage_internal()->cleanup().UnregisterObject(this);
    }
    delete internal_;
    internal_ = nullptr;
  }

  // Copy from other
  if (other.internal_) {
    internal_ = new internal::ListResultInternal(*other.internal_);
  }

  // Register new internal object
  if (internal_ && internal_->storage_internal() &&
      internal_->storage_internal()->app_valid()) {
    internal_->storage_internal()->cleanup().RegisterObject(
        this, GlobalCleanupListResult);
  }
  return *this;
}

ListResult::ListResult(ListResult&& other) : internal_(nullptr) {
  // Unregister 'other' as it will no longer manage its internal_
  if (other.internal_ && other.internal_->storage_internal() &&
      other.internal_->storage_internal()->app_valid()) {
    other.internal_->storage_internal()->cleanup().UnregisterObject(&other);
  }

  // Move internal pointer
  internal_ = other.internal_;
  other.internal_ = nullptr;

  // Register 'this' if it now owns a valid internal object
  if (internal_ && internal_->storage_internal() &&
      internal_->storage_internal()->app_valid()) {
    internal_->storage_internal()->cleanup().RegisterObject(
        this, GlobalCleanupListResult);
  }
}

ListResult& ListResult::operator=(ListResult&& other) {
  if (this == &other) {
    return *this;
  }

  // Unregister and delete current internal object for 'this'
  if (internal_) {
    if (internal_->storage_internal() &&
        internal_->storage_internal()->app_valid()) {
      internal_->storage_internal()->cleanup().UnregisterObject(this);
    }
    delete internal_;
    internal_ = nullptr;
  }

  // Unregister 'other' as it will no longer manage its internal_
  if (other.internal_ && other.internal_->storage_internal() &&
      other.internal_->storage_internal()->app_valid()) {
    other.internal_->storage_internal()->cleanup().UnregisterObject(&other);
  }

  // Move internal pointer
  internal_ = other.internal_;
  other.internal_ = nullptr;

  // Register 'this' if it now owns a valid internal object
  if (internal_ && internal_->storage_internal() &&
      internal_->storage_internal()->app_valid()) {
    internal_->storage_internal()->cleanup().RegisterObject(
        this, GlobalCleanupListResult);
  }
  return *this;
}

ListResult::~ListResult() {
  if (internal_) {
    if (internal_->storage_internal() &&
        internal_->storage_internal()->app_valid()) {
      internal_->storage_internal()->cleanup().UnregisterObject(this);
    }
    delete internal_;
    internal_ = nullptr;
  }
}

void ListResult::ClearInternalForCleanup() {
  // This method is called by GlobalCleanupListResult.
  // The object is already unregistered from the CleanupNotifier by the notifier
  // itself before this callback is invoked. So, no need to call
  // UnregisterObject here.
  delete internal_;
  internal_ = nullptr;
}

std::vector<StorageReference> ListResult::items() const {
  assert(internal_ != nullptr);
  if (!internal_) return std::vector<StorageReference>();
  return internal_->items();
}

std::vector<StorageReference> ListResult::prefixes() const {
  assert(internal_ != nullptr);
  if (!internal_) return std::vector<StorageReference>();
  return internal_->prefixes();
}

std::string ListResult::page_token() const {
  assert(internal_ != nullptr);
  if (!internal_) return "";
  return internal_->page_token();
}

bool ListResult::is_valid() const { return internal_ != nullptr; }

}  // namespace storage
}  // namespace firebase
