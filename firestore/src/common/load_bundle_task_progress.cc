/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/include/firebase/firestore/load_bundle_task_progress.h"

#include <stdint.h>

#include "firestore/src/common/hard_assert_common.h"
#if defined(__ANDROID__)
#include "firestore/src/android/load_bundle_task_progress_android.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

LoadBundleTaskProgress::LoadBundleTaskProgress(int32_t documents_loaded,
                                               int32_t total_documents,
                                               int64_t bytes_loaded,
                                               int64_t total_bytes,
                                               State state)
    : documents_loaded_(documents_loaded),
      total_documents_(total_documents),
      bytes_loaded_(bytes_loaded),
      total_bytes_(total_bytes),
      state_(state) {}

// Android requires below constructor to create this object from internal
// objects in a promise. See promise_android.h
#if defined(__ANDROID__)
LoadBundleTaskProgress::LoadBundleTaskProgress(
    LoadBundleTaskProgressInternal* internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  documents_loaded_ = internal->documents_loaded();
  total_documents_ = internal->total_documents();
  bytes_loaded_ = internal->bytes_loaded();
  total_bytes_ = internal->total_bytes();
  state_ = internal->state();

  delete internal;
}
#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
