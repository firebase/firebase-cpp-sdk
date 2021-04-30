#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRITE_BATCH_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRITE_BATCH_ANDROID_H_

#include "firestore/src/android/promise_factory_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/write_batch.h"

namespace firebase {
namespace firestore {

class WriteBatchInternal : public Wrapper {
 public:
  // Each API of WriteBatch that returns a Future needs to define an enum value
  // here. For example, a Future-returning method Foo() relies on the enum value
  // kFoo. The enum values are used to identify and manage Future in the
  // Firestore Future manager.
  enum class AsyncFn {
    kCommit = 0,
    kCount,  // Must be the last enum value.
  };

  static void Initialize(jni::Loader& loader);

  WriteBatchInternal(FirestoreInternal* firestore, const jni::Object& object)
      : Wrapper(firestore, object), promises_(firestore) {}

  void Set(const DocumentReference& document, const MapFieldValue& data,
           const SetOptions& options);

  void Update(const DocumentReference& document, const MapFieldValue& data);

  void Update(const DocumentReference& document, const MapFieldPathValue& data);

  void Delete(const DocumentReference& document);

  Future<void> Commit();

 private:
  /**
   * Converts a public DocumentReference to a non-owning proxy for its backing
   * Java object. The Java object is owned by the DocumentReference.
   *
   * Note: this method is not visible to `Env`, so this must still be invoked
   * manually for arguments passed to `Env` methods.
   */
  // TODO(mcg): Move this out of WriteBatchInternal
  // This needs to be here now because of existing friend relationships.
  static jni::Object ToJava(const DocumentReference& reference);

  PromiseFactory<AsyncFn> promises_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_WRITE_BATCH_ANDROID_H_
