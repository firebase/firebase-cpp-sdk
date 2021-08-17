/*
 * Copyright 2020 Google LLC
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

#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"

#include <ostream>

namespace firebase {
namespace firestore {

std::string SnapshotMetadata::ToString() const {
  return std::string("SnapshotMetadata{") +
         "has_pending_writes=" + (has_pending_writes() ? "true" : "false") +
         ", is_from_cache=" + (is_from_cache() ? "true" : "false") + '}';
}

std::ostream& operator<<(std::ostream& out, const SnapshotMetadata& metadata) {
  return out << metadata.ToString();
}

}  // namespace firestore
}  // namespace firebase
