// Copyright 2023 Google LLC
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

#ifndef FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_COMMON_H_
#define FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_COMMON_H_

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "storage/src/common/storage_internal.h" // Required for ListResultInternal definition

// Forward declare ListResultInternal from its platform specific location
// This header is included by platform specific ListResultInternal headers.
namespace firebase {
namespace storage {
namespace internal {
class ListResultInternal; // Forward declaration
}  // namespace internal
}  // namespace storage
}  // namespace firebase


namespace firebase {
namespace storage {
namespace internal {

// This class is used to manage the lifetime of ListResultInternal objects.
// When a ListResult object is created, it creates a ListResultInternalCommon
// object that registers itself with the CleanupNotifier. When the App is
// destroyed, the CleanupNotifier will delete all registered
// ListResultInternalCommon objects, which in turn delete their associated
// ListResultInternal objects.
class ListResultInternalCommon {
 public:
  explicit ListResultInternalCommon(ListResultInternal* internal);
  ~ListResultInternalCommon();

  ListResultInternal* internal() const { return internal_; }

  // Finds the ListResultInternalCommon object associated with the given
  // ListResultInternal object.
  static ListResultInternalCommon* FindListResultInternalCommon(
      ListResultInternal* internal);

 private:
  CleanupNotifier* GetCleanupNotifier();

  ListResultInternal* internal_;
  // We need the App to find the CleanupNotifier.
  // ListResultInternal owns StorageInternal, which owns App.
  App* app_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_COMMON_H_
