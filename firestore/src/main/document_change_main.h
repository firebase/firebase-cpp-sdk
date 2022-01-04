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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_CHANGE_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_CHANGE_MAIN_H_

#include <cstdint>

#include "Firestore/core/src/api/document_change.h"
#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/main/firestore_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

class DocumentChangeInternal {
 public:
  explicit DocumentChangeInternal(api::DocumentChange&& change);

  FirestoreInternal* firestore_internal();

  DocumentChange::Type type() const;
  DocumentSnapshot document() const;
  std::size_t old_index() const;
  std::size_t new_index() const;

  std::size_t Hash() const { return change_.Hash(); }

  friend bool operator==(const DocumentChangeInternal& lhs,
                         const DocumentChangeInternal& rhs);

 private:
  api::DocumentChange change_;
};

inline bool operator!=(const DocumentChangeInternal& lhs,
                       const DocumentChangeInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_CHANGE_MAIN_H_
