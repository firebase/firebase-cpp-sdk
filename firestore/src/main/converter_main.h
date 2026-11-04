// Copyright 2021 Google LLC
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
#include "firebase/firestore/aggregate_query.h"
#include "firebase/firestore/aggregate_query_snapshot.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/main/aggregate_query_main.h"
#include "firestore/src/main/aggregate_query_snapshot_main.h"
#include "firestore/src/main/collection_reference_main.h"
#include "firestore/src/main/composite_filter_main.h"
#include "firestore/src/main/document_change_main.h"
#include "firestore/src/main/document_reference_main.h"
#include "firestore/src/main/document_snapshot_main.h"
#include "firestore/src/main/field_value_main.h"
#include "firestore/src/main/listener_registration_main.h"
#include "firestore/src/main/query_main.h"
#include "firestore/src/main/query_snapshot_main.h"
#include "firestore/src/main/transaction_main.h"
#include "firestore/src/main/unary_filter_main.h"
#include "firestore/src/main/write_batch_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase::firestore {

template <>
struct InternalTypeMap<FieldPath> {
  using type = model::FieldPath;
};

struct ConverterImpl {
  template <typename PublicT, typename ArgT>
  static PublicT MakePublicFromInternal(ArgT&& from) {
    return PublicT{new typename std::decay<ArgT>::type(std::forward<ArgT>(from))};
  }

  template <typename PublicT, typename CoreT, typename InternalT = InternalType<PublicT>,
            typename... Args>
  static PublicT MakePublicFromCore(CoreT&& from, Args&&... args) {
    return PublicT{new InternalT(std::forward<CoreT>(from), std::forward<Args>(args)...)};
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static InternalT* GetInternal(PublicT* from) {
    return from ? from->internal_ : nullptr;
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static const InternalT* GetInternal(const PublicT* from) {
    return from ? from->internal_ : nullptr;
  }
};

inline AggregateQuery MakePublic(api::AggregateQuery&& from) {
  return ConverterImpl::MakePublicFromCore<AggregateQuery>(std::move(from));
}
inline CollectionReference MakePublic(api::CollectionReference&& from) {
  return ConverterImpl::MakePublicFromCore<CollectionReference>(std::move(from));
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
inline Filter MakePublic(UnaryFilterInternal&& from) {
  return ConverterImpl::MakePublicFromInternal<Filter>(std::move(from));
}
inline Filter MakePublic(CompositeFilterInternal&& from) {
  return ConverterImpl::MakePublicFromInternal<Filter>(std::move(from));
}
inline FieldValue MakePublic(FieldValueInternal&& from) {
  return ConverterImpl::MakePublicFromInternal<FieldValue>(std::move(from));
}
inline AggregateQuerySnapshot MakePublic(AggregateQuerySnapshotInternal&& from) {
  return ConverterImpl::MakePublicFromInternal<AggregateQuerySnapshot>(std::move(from));
}
inline ListenerRegistration MakePublic(std::unique_ptr<api::ListenerRegistration> from,
                                       FirestoreInternal* firestore) {
  return ConverterImpl::MakePublicFromCore<ListenerRegistration>(std::move(from), firestore);
}
inline Query MakePublic(api::Query&& from) {
  return ConverterImpl::MakePublicFromCore<Query>(std::move(from));
}
inline QuerySnapshot MakePublic(api::QuerySnapshot&& from) {
  return ConverterImpl::MakePublicFromCore<QuerySnapshot>(std::move(from));
}
inline Transaction MakePublic(std::shared_ptr<core::Transaction> transaction,
                              std::shared_ptr<api::Firestore> firestore_core,
                              Firestore* firestore_public) {
  return ConverterImpl::MakePublicFromCore<Transaction>(
      std::move(transaction), std::move(firestore_core), firestore_public);
}
inline WriteBatch MakePublic(api::WriteBatch&& from) {
  return ConverterImpl::MakePublicFromCore<WriteBatch>(std::move(from));
}

template <typename PublicT, typename InternalT = InternalType<PublicT>>
InternalT* GetInternal(PublicT* from) {
  return ConverterImpl::GetInternal(from);
}

template <typename PublicT, typename InternalT = InternalType<PublicT>>
const InternalT* GetInternal(const PublicT* from) {
  return ConverterImpl::GetInternal(from);
}

inline const model::FieldPath& GetInternal(const FieldPath& from) {
  return *NOT_NULL(ConverterImpl::GetInternal(&from));
}

inline const api::DocumentReference& GetCoreApi(const DocumentReference& from) {
  return NOT_NULL(GetInternal(&from))->document_reference_core();
}

inline const api::DocumentSnapshot& GetCoreApi(const DocumentSnapshot& from) {
  return NOT_NULL(GetInternal(&from))->document_snapshot_core();
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_CONVERTER_MAIN_H_
