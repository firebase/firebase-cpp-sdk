#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRITE_BATCH_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRITE_BATCH_ANDROID_H_

#include "firestore/src/android/wrapper_future.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/write_batch.h"

namespace firebase {
namespace firestore {

// Each API of WriteBatch that returns a Future needs to define an enum value
// here. For example, Foo() and FooLastResult() implementation relies on the
// enum value kFoo. The enum values are used to identify and manage Future in
// the Firestore Future manager.
enum class WriteBatchFn {
  kCommit = 0,
  kCount,  // Must be the last enum value.
};

class WriteBatchInternal
    : public WrapperFuture<WriteBatchFn, WriteBatchFn::kCount> {
 public:
  using ApiType = WriteBatch;
  using WrapperFuture::WrapperFuture;

  void Set(const DocumentReference& document, const MapFieldValue& data,
           const SetOptions& options);

  void Update(const DocumentReference& document, const MapFieldValue& data);

  void Update(const DocumentReference& document, const MapFieldPathValue& data);

  void Delete(const DocumentReference& document);

  Future<void> Commit();

  Future<void> CommitLastResult();

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRITE_BATCH_ANDROID_H_
