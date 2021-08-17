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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_COLLECTION_REFERENCE_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_COLLECTION_REFERENCE_MAIN_H_

#include <string>

#include "Firestore/core/src/api/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/main/promise_factory_main.h"
#include "firestore/src/main/query_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

class CollectionReferenceInternal : public QueryInternal {
 public:
  explicit CollectionReferenceInternal(api::CollectionReference&& collection);

  const std::string& id() const;
  std::string path() const;

  DocumentReference Parent() const;
  DocumentReference Document() const;
  DocumentReference Document(const std::string& document_path) const;

  Future<DocumentReference> Add(const MapFieldValue& data);

 private:
  const api::CollectionReference& collection_core_api() const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_COLLECTION_REFERENCE_MAIN_H_
