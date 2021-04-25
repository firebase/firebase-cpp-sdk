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
#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LOAD_BUNDLE_TASK_PROGRESS_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LOAD_BUNDLE_TASK_PROGRESS_IOS_H_

#include <cstdint>

namespace firebase {
namespace firestore {

class LoadBundleTaskProgressInternal {
 public:
  int32_t documents_loaded() const { return documents_loaded_; }

  int32_t total_documents() const { return total_documents_; }

  int64_t bytes_loaded() const { return bytes_loaded_; }

  int64_t total_bytes() const { return total_bytes_; }

  LoadBundleTaskProgress::State state() const { return state_; }

 private:
  int32_t documents_loaded_;
  int32_t total_documents_;
  int64_t bytes_loaded_;
  int64_t total_bytes_;
  LoadBundleTaskProgress::State state_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LOAD_BUNDLE_TASK_PROGRESS_IOS_H_
