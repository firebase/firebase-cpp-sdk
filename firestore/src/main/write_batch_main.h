// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_WRITE_BATCH_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_WRITE_BATCH_MAIN_H_

#include "Firestore/core/src/api/write_batch.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/write_batch.h"
#include "firestore/src/main/firestore_main.h"
#include "firestore/src/main/promise_factory_main.h"
#include "firestore/src/main/user_data_converter_main.h"

namespace firebase {
namespace firestore {

class WriteBatchInternal {
 public:
  explicit WriteBatchInternal(api::WriteBatch&& batch);

  Firestore* firestore();
  FirestoreInternal* firestore_internal();

  void Set(const DocumentReference& document,
           const MapFieldValue& data,
           const SetOptions& options);

  void Update(const DocumentReference& document, const MapFieldValue& data);
  void Update(const DocumentReference& document, const MapFieldPathValue& data);

  void Delete(const DocumentReference& document);

  Future<void> Commit();

 private:
  enum class AsyncApis {
    kCommit,
    kCount,
  };

  api::WriteBatch batch_;
  PromiseFactory<AsyncApis> promise_factory_;
  UserDataConverter user_data_converter_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_WRITE_BATCH_MAIN_H_
