/*
 * Copyright 2018 Google
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

#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_FIRESTORE_METADATA_CHANGES_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_FIRESTORE_METADATA_CHANGES_H_

namespace firebase {
namespace firestore {

/**
 * Indicates whether metadata-only changes (that is,
 * DocumentSnapshot::metadata() or QuerySnapshot::metadata() changed) should
 * trigger snapshot events.
 */
enum class MetadataChanges {
  /** Snapshot events will not be triggered by metadata-only changes. */
  kExclude,

  /**
   * Snapshot events will be triggered by any changes, including metadata-only
   * changes.
   */
  kInclude,
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_FIRESTORE_METADATA_CHANGES_H_
