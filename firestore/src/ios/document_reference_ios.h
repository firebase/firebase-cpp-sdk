#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_REFERENCE_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_REFERENCE_IOS_H_

#include <functional>
#include <memory>
#include <string>

#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/ios/firestore_ios.h"
#include "firestore/src/ios/promise_factory_ios.h"
#include "firestore/src/ios/user_data_converter_ios.h"
#include "Firestore/core/src/api/document_reference.h"

namespace firebase {
namespace firestore {

namespace core {
class ParsedSetData;
class ParsedUpdateData;
}  // namespace core

class Firestore;
class FirestoreInternal;

class DocumentReferenceInternal {
 public:
  explicit DocumentReferenceInternal(api::DocumentReference&& reference);

  Firestore* firestore();
  FirestoreInternal* firestore_internal();

  const std::string& id() const { return reference_.document_id(); }
  std::string path() const { return reference_.Path(); }
  const model::DocumentKey& key() const { return reference_.key(); }

  CollectionReference Parent();
  CollectionReference Collection(const std::string& collection_path);

  Future<DocumentSnapshot> Get(Source source);

  Future<void> Set(const MapFieldValue& data, const SetOptions& options);

  Future<void> Update(const MapFieldValue& data);
  Future<void> Update(const MapFieldPathValue& data);

  Future<void> Delete();

  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      EventListener<DocumentSnapshot>* listener);

  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::function<void(const DocumentSnapshot&, Error, const std::string&)>
          callback);

  const api::DocumentReference& document_reference_core() const {
    return reference_;
  }

 private:
  enum class AsyncApis {
    kGet,
    kSet,
    kUpdate,
    kDelete,
    kCount,
  };

  Future<void> UpdateImpl(core::ParsedUpdateData&& parsed);

  api::DocumentReference reference_;
  PromiseFactory<AsyncApis> promise_factory_;
  UserDataConverter user_data_converter_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_REFERENCE_IOS_H_
