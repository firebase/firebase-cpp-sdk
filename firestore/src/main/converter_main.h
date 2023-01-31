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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_CONVERTER_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_CONVERTER_MAIN_H_

#include <memory>
#include <utility>

#include "Firestore/core/src/api/aggregate_query.h"
#include "Firestore/core/src/api/collection_reference.h"
#include "Firestore/core/src/api/document_change.h"
#include "Firestore/core/src/api/document_reference.h"
#include "Firestore/core/src/api/document_snapshot.h"
#include "Firestore/core/src/api/listener_registration.h"
#include "Firestore/core/src/api/query_core.h"
#include "Firestore/core/src/api/query_snapshot.h"
#include "Firestore/core/src/core/transaction.h"
#include "Firestore/core/src/model/field_path.h"
#include "absl/memory/memory.h"
#include "firebase/firestore/aggregate_query.h"
#include "firebase/firestore/aggregate_query_snapshot.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/main/aggregate_query_main.h"
#include "firestore/src/main/aggregate_query_snapshot_main.h"
#include "firestore/src/main/collection_reference_main.h"
#include "firestore/src/main/document_change_main.h"
#include "firestore/src/main/document_reference_main.h"
#include "firestore/src/main/document_snapshot_main.h"
#include "firestore/src/main/field_value_main.h"
#include "firestore/src/main/listener_registration_main.h"
#include "firestore/src/main/query_main.h"
#include "firestore/src/main/query_snapshot_main.h"
#include "firestore/src/main/transaction_main.h"
#include "firestore/src/main/write_batch_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

// Additional specializations of InternalTypeMap for iOS.
template <>
struct InternalTypeMap<FieldPath> {
  using type = model::FieldPath;
};

// The struct is just to make declaring `MakePublic` a friend easier (and
// future-proof in case more parameters are added to it).
struct ConverterImpl {
  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static PublicT MakePublicFromInternal(InternalT&& from) {
    auto* internal = new InternalT(std::move(from));
    return PublicT{internal};
  }

  template <typename PublicT,
            typename CoreT,
            typename InternalT = InternalType<PublicT>,
            typename... Args>
  static PublicT MakePublicFromCore(CoreT&& from, Args... args) {
    auto* internal = new InternalT(std::move(from), std::move(args)...);
    return PublicT{internal};
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static InternalT* GetInternal(PublicT* from) {
    return from->internal_;
  }
};

// MakePublic

inline AggregateQuery MakePublic(api::AggregateQuery&& from) {
  return ConverterImpl::MakePublicFromCore<AggregateQuery>(std::move(from));
}

inline CollectionReference MakePublic(api::CollectionReference&& from) {
  return ConverterImpl::MakePublicFromCore<CollectionReference>(
      std::move(from));
}

inline DocumentChange MakePublic(api::DocumentChange&& from) {
  return ConverterImpl::MakePublicFromCore<DocumentChange>(std::move(from));
}

inline DocumentReference MakePublic(api::DocumentReference&& from) {
  return ConverterImpl::MakePublicFromCore<DocumentReference>(std::move(from));
}

inline DocumentSnapshot MakePublic(api::DocumentSnapshot&& from) {
  return ConverterImpl::MakePublicFromCore<DocumentSnapshot>(std::move(from));
}

inline FieldValue MakePublic(FieldValueInternal&& from) {
  return ConverterImpl::MakePublicFromInternal<FieldValue>(std::move(from));
}

inline AggregateQuerySnapshot MakePublic(
    AggregateQuerySnapshotInternal&& from) {
  return ConverterImpl::MakePublicFromInternal<AggregateQuerySnapshot>(
      std::move(from));
}

inline ListenerRegistration MakePublic(
    std::unique_ptr<api::ListenerRegistration> from,
    FirestoreInternal* firestore) {
  return ConverterImpl::MakePublicFromCore<ListenerRegistration>(
      std::move(from), firestore);
}

inline Query MakePublic(api::Query&& from) {
  return ConverterImpl::MakePublicFromCore<Query>(from);
}

inline QuerySnapshot MakePublic(api::QuerySnapshot&& from) {
  return ConverterImpl::MakePublicFromCore<QuerySnapshot>(from);
}

// TODO(c++17): Add a `MakePublic` overload for `Transaction`, which is not
// copy- or move-constructible (it's okay to return such a class from a function
// in C++17, but not in prior versions).

inline WriteBatch MakePublic(api::WriteBatch&& from) {
  return ConverterImpl::MakePublicFromCore<WriteBatch>(std::move(from));
}

// GetInternal

template <typename PublicT, typename InternalT = InternalType<PublicT>>
InternalT* GetInternal(PublicT* from) {
  return ConverterImpl::GetInternal(from);
}

inline const model::FieldPath& GetInternal(const FieldPath& from) {
  return *ConverterImpl::GetInternal(&from);
}

// GetCoreApi

inline const api::DocumentReference& GetCoreApi(const DocumentReference& from) {
  return GetInternal(&from)->document_reference_core();
}

inline const api::DocumentSnapshot& GetCoreApi(const DocumentSnapshot& from) {
  return GetInternal(&from)->document_snapshot_core();
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_CONVERTER_MAIN_H_
