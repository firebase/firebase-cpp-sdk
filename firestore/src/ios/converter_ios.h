#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CONVERTER_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CONVERTER_IOS_H_

#include <memory>
#include <utility>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/ios/collection_reference_ios.h"
#include "firestore/src/ios/document_change_ios.h"
#include "firestore/src/ios/document_reference_ios.h"
#include "firestore/src/ios/document_snapshot_ios.h"
#include "firestore/src/ios/field_value_ios.h"
#include "firestore/src/ios/listener_registration_ios.h"
#include "firestore/src/ios/query_ios.h"
#include "firestore/src/ios/query_snapshot_ios.h"
#include "firestore/src/ios/transaction_ios.h"
#include "firestore/src/ios/write_batch_ios.h"
#include "absl/memory/memory.h"
#include "Firestore/core/src/firebase/firestore/api/collection_reference.h"
#include "Firestore/core/src/firebase/firestore/api/document_change.h"
#include "Firestore/core/src/firebase/firestore/api/document_reference.h"
#include "Firestore/core/src/firebase/firestore/api/document_snapshot.h"
#include "Firestore/core/src/firebase/firestore/api/listener_registration.h"
#include "Firestore/core/src/firebase/firestore/api/query_core.h"
#include "Firestore/core/src/firebase/firestore/api/query_snapshot.h"
#include "Firestore/core/src/firebase/firestore/core/transaction.h"
#include "Firestore/core/src/firebase/firestore/model/field_path.h"

namespace firebase {
namespace firestore {

// The struct is just to make declaring `MakePublic` a friend easier (and
// future-proof in case more parameters are added to it).
struct ConverterImpl {
  template <typename PublicT, typename InternalT>
  static PublicT MakePublicFromInternal(InternalT&& from) {
    auto* internal = new InternalT(std::move(from));
    return PublicT{internal};
  }

  template <typename PublicT, typename InternalT, typename CoreT,
            typename... Args>
  static PublicT MakePublicFromCore(CoreT&& from, Args... args) {
    auto* internal = new InternalT(std::move(from), std::move(args)...);
    return PublicT{internal};
  }

  template <typename InternalT, typename PublicT>
  static InternalT* GetInternal(PublicT* from) {
    return from->internal_;
  }
};

// MakePublic

inline CollectionReference MakePublic(api::CollectionReference&& from) {
  return ConverterImpl::MakePublicFromCore<CollectionReference,
                                           CollectionReferenceInternal>(
      std::move(from));
}

inline DocumentChange MakePublic(api::DocumentChange&& from) {
  return ConverterImpl::MakePublicFromCore<DocumentChange,
                                           DocumentChangeInternal>(
      std::move(from));
}

inline DocumentReference MakePublic(api::DocumentReference&& from) {
  return ConverterImpl::MakePublicFromCore<DocumentReference,
                                           DocumentReferenceInternal>(
      std::move(from));
}

inline DocumentSnapshot MakePublic(api::DocumentSnapshot&& from) {
  return ConverterImpl::MakePublicFromCore<DocumentSnapshot,
                                           DocumentSnapshotInternal>(
      std::move(from));
}

inline FieldValue MakePublic(FieldValueInternal&& from) {
  return ConverterImpl::MakePublicFromInternal<FieldValue>(std::move(from));
}

inline ListenerRegistration MakePublic(
    std::unique_ptr<api::ListenerRegistration> from,
    FirestoreInternal* firestore) {
  return ConverterImpl::MakePublicFromCore<ListenerRegistration,
                                           ListenerRegistrationInternal>(
      std::move(from), firestore);
}

inline Query MakePublic(api::Query&& from) {
  return ConverterImpl::MakePublicFromCore<Query, QueryInternal>(from);
}

inline QuerySnapshot MakePublic(api::QuerySnapshot&& from) {
  return ConverterImpl::MakePublicFromCore<QuerySnapshot,
                                           QuerySnapshotInternal>(from);
}

// TODO(c++17): Add a `MakePublic` overload for `Transaction`, which is not
// copy- or move-constructible (it's okay to return such a class from a function
// in C++17, but not in prior versions).

inline WriteBatch MakePublic(api::WriteBatch&& from) {
  return ConverterImpl::MakePublicFromCore<WriteBatch, WriteBatchInternal>(
      std::move(from));
}

// GetInternal

inline FirestoreInternal* GetInternal(Firestore* from) {
  return ConverterImpl::GetInternal<FirestoreInternal>(from);
}

inline FieldValueInternal* GetInternal(FieldValue* from) {
  return ConverterImpl::GetInternal<FieldValueInternal>(from);
}

inline const FieldValueInternal* GetInternal(const FieldValue* from) {
  return ConverterImpl::GetInternal<FieldValueInternal>(from);
}

inline DocumentReferenceInternal* GetInternal(DocumentReference* from) {
  return ConverterImpl::GetInternal<DocumentReferenceInternal>(from);
}

inline const DocumentSnapshotInternal* GetInternal(
    const DocumentSnapshot* from) {
  return ConverterImpl::GetInternal<DocumentSnapshotInternal>(from);
}

inline const DocumentReferenceInternal* GetInternal(
    const DocumentReference* from) {
  return ConverterImpl::GetInternal<DocumentReferenceInternal>(from);
}

inline const model::FieldPath& GetInternal(const FieldPath& from) {
  return *ConverterImpl::GetInternal<model::FieldPath>(&from);
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

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CONVERTER_IOS_H_
