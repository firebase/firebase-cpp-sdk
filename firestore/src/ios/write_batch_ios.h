#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_WRITE_BATCH_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_WRITE_BATCH_IOS_H_

#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/write_batch.h"
#include "firestore/src/ios/firestore_ios.h"
#include "firestore/src/ios/promise_factory_ios.h"
#include "firestore/src/ios/user_data_converter_ios.h"
#include "Firestore/core/src/api/write_batch.h"

namespace firebase {
namespace firestore {

class WriteBatchInternal {
 public:
  explicit WriteBatchInternal(api::WriteBatch&& batch);

  Firestore* firestore();
  FirestoreInternal* firestore_internal();

  void Set(const DocumentReference& document, const MapFieldValue& data,
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

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_WRITE_BATCH_IOS_H_
