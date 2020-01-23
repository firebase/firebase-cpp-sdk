#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_COLLECTION_REFERENCE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_COLLECTION_REFERENCE_ANDROID_H_

#include <jni.h>

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/query_android.h"
#include "firestore/src/android/wrapper_future.h"

namespace firebase {
namespace firestore {

// To make things simple, CollectionReferenceInternal uses the Future management
// from its base class, QueryInternal. Each API of CollectionReference that
// returns a Future needs to define an enum value to QueryFn. For example, Foo()
// and FooLastResult() implementation relies on the enum value QueryFn::kFoo.
// The enum values are used to identify and manage Future in the Firestore
// Future manager.
using CollectionReferenceFn = QueryFn;

// This is the Android implementation of CollectionReference.
class CollectionReferenceInternal : public QueryInternal {
 public:
  using ApiType = CollectionReference;
  using QueryInternal::QueryInternal;

  const std::string& id() const;
  const std::string& path() const;
  DocumentReference Parent() const;
  DocumentReference Document() const;
  DocumentReference Document(const std::string& document_path) const;
  Future<DocumentReference> Add(const MapFieldValue& data);
  Future<DocumentReference> AddLastResult();

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);

  // Below are cached call results.
  mutable std::string cached_id_;
  mutable std::string cached_path_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_COLLECTION_REFERENCE_ANDROID_H_
