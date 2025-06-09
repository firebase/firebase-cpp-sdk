// Copyright 2021 Google LLC
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

// Copyright 2021 Google LLC
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

#include "storage/src/common/list_result_internal_common.h"
#include "storage/src/common/storage_reference_internal.h" // For StorageReference constructor from internal
#include "app/src/include/firebase/app.h" // For App, LogDebug
#include "app/src/util.h" // For LogDebug

// Platform specific ListResultInternal definitions
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/list_result_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "storage/src/ios/list_result_ios.h"
#elif FIREBASE_PLATFORM_DESKTOP
#include "storage/src/desktop/list_result_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS, FIREBASE_PLATFORM_DESKTOP


namespace firebase {
namespace storage {

using internal::ListResultInternal;
using internal::ListResultInternalCommon;

ListResult::ListResult() : internal_(nullptr) {}

ListResult::ListResult(ListResultInternal* internal)
    : internal_(internal) {
  if (internal_) {
    // Create a ListResultInternalCommon to manage the lifetime of the
    // internal object.
    new ListResultInternalCommon(internal_);
  }
}

ListResult::ListResult(const ListResult& other) : internal_(nullptr) {
  if (other.internal_) {
    // Create a new internal object by copying the other's internal object.
    internal_ = new ListResultInternal(*other.internal_);
    // Create a ListResultInternalCommon to manage the lifetime of the new
    // internal object.
    new ListResultInternalCommon(internal_);
  }
}

ListResult& ListResult::operator=(const ListResult& other) {
  if (&other == this) {
    return *this;
  }
  // If this object already owns an internal object, delete it via its
  // ListResultInternalCommon.
  if (internal_) {
    ListResultInternalCommon* common =
        ListResultInternalCommon::FindListResultInternalCommon(internal_);
    // This will delete internal_ as well through the cleanup mechanism.
    delete common;
    internal_ = nullptr;
  }
  if (other.internal_) {
    // Create a new internal object by copying the other's internal object.
    internal_ = new ListResultInternal(*other.internal_);
    // Create a ListResultInternalCommon to manage the lifetime of the new
    // internal object.
    new ListResultInternalCommon(internal_);
  }
  return *this;
}

ListResult::ListResult(ListResult&& other) : internal_(other.internal_) {
  // The internal object is now owned by this object, so null out other's
  // pointer. The ListResultInternalCommon associated with the internal object
  // is still valid and now refers to this object's internal_.
  other.internal_ = nullptr;
}

ListResult& ListResult::operator=(ListResult&& other) {
  if (&other == this) {
    return *this;
  }
  // Delete our existing internal object if it exists.
  if (internal_) {
    ListResultInternalCommon* common =
        ListResultInternalCommon::FindListResultInternalCommon(internal_);
    // This will delete internal_ as well through the cleanup mechanism.
    delete common;
  }
  // Transfer ownership of the internal object from other to this.
  internal_ = other.internal_;
  other.internal_ = nullptr;
  return *this;
}

ListResult::~ListResult() {
  if (internal_) {
    ListResultInternalCommon* common =
        ListResultInternalCommon::FindListResultInternalCommon(internal_);
    // This will delete internal_ as well through the cleanup mechanism.
    delete common;
    internal_ = nullptr;
  }
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

namespace internal {

ListResultInternalCommon::ListResultInternalCommon(
    ListResultInternal* internal)
    : internal_(internal), app_(internal->storage_internal()->app()) {
  GetCleanupNotifier()->RegisterObject(this, [](void* object) {
    ListResultInternalCommon* common =
        reinterpret_cast<ListResultInternalCommon*>(object);
    LogDebug(
        "ListResultInternalCommon object %p (internal %p) deleted by "
        "CleanupNotifier",
        common, common->internal_);
    delete common->internal_;  // Delete the platform-specific internal object.
    delete common;             // Delete this common manager.
  });
  LogDebug(
      "ListResultInternalCommon object %p (internal %p) registered with "
      "CleanupNotifier",
      this, internal_);
}

ListResultInternalCommon::~ListResultInternalCommon() {
  LogDebug(
      "ListResultInternalCommon object %p (internal %p) unregistered from "
      "CleanupNotifier and being deleted",
      this, internal_);
  GetCleanupNotifier()->UnregisterObject(this);
  // internal_ is not deleted here; it's deleted by the CleanupNotifier callback
  // or when the ListResult itself is destroyed if cleanup already ran.
  internal_ = nullptr;
}

CleanupNotifier* ListResultInternalCommon::GetCleanupNotifier() {
  assert(app_ != nullptr);
  return CleanupNotifier::FindByOwner(app_);
}

ListResultInternalCommon* ListResultInternalCommon::FindListResultInternalCommon(
    ListResultInternal* internal_to_find) {
  if (internal_to_find == nullptr ||
      internal_to_find->storage_internal() == nullptr ||
      internal_to_find->storage_internal()->app() == nullptr) {
    return nullptr;
  }
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(
      internal_to_find->storage_internal()->app());
  if (notifier == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<ListResultInternalCommon*>(
      notifier->FindObject(internal_to_find, [](void* object, void* search_object) {
        ListResultInternalCommon* common_obj =
            reinterpret_cast<ListResultInternalCommon*>(object);
        return common_obj->internal() ==
               reinterpret_cast<ListResultInternal*>(search_object);
      }));
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
