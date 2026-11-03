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

#include "firestore/src/main/collection_reference_main.h"

#include <utility>

#include "Firestore/core/src/core/user_data.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/document_reference_main.h"
#include "firestore/src/main/field_value_main.h"
#include "firestore/src/main/user_data_converter_main.h"

namespace firebase {
namespace firestore {

using core::ParsedSetData;

CollectionReferenceInternal::CollectionReferenceInternal(
    api::CollectionReference&& collection)
    : QueryInternal{api::Query{collection}},
      collection_{std::move(collection)} {}

const std::string& CollectionReferenceInternal::id() const {
  return collection_core_api().collection_id();
}

std::string CollectionReferenceInternal::path() const {
  return collection_core_api().path();
}

DocumentReference CollectionReferenceInternal::Parent() const {
  absl::optional<api::DocumentReference> parent =
      collection_core_api().parent();
  if (parent) {
    return MakePublic(std::move(*parent));
  } else {
    return DocumentReference{};
  }
}

DocumentReference CollectionReferenceInternal::Document() const {
  return MakePublic(collection_core_api().Document());
}

DocumentReference CollectionReferenceInternal::Document(
    const std::string& document_path) const {
  if (document_path.find("//") != std::string::npos) {
    SimpleThrowInvalidArgument(std::string("Invalid path (") + document_path +
                               "). Paths must not contain // in them.");
  }
  return MakePublic(collection_core_api().Document(document_path));
}

Future<DocumentReference> CollectionReferenceInternal::Add(
    const MapFieldValue& data) {
  ParsedSetData parsed = converter().ParseSetData(data);
  auto promise = promise_factory().CreatePromise<DocumentReference>(
      AsyncApis::kCollectionReferenceAdd);

  api::DocumentReference new_doc = collection_core_api().Document();
  auto callback = [promise, new_doc](const util::Status& status) mutable {
    if (status.ok()) {
      promise.SetValue(MakePublic(std::move(new_doc)));
    } else {
      promise.SetError(status);
    }
  };

  new_doc.SetData(std::move(parsed), std::move(callback));
  return promise.future();
}

}  // namespace firestore
}  // namespace firebase
