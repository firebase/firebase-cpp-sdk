// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_REFERENCE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_REFERENCE_ANDROID_H_

#include <mutex>
#include <string>

#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/promise_factory_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase::firestore {

class Firestore;

class DocumentReferenceInternal : public Wrapper {
 public:
  enum class AsyncFn {
    kGet = 0,
    kSet,
    kUpdate,
    kUpdateFieldPath,
    kDelete,
    kCount,
  };

  static DocumentReference Create(jni::Env& env, const jni::Object& reference);

  DocumentReferenceInternal(FirestoreInternal* firestore,
                            const jni::Object& object)
      : Wrapper(firestore, object), promises_(firestore) {}

  DocumentReferenceInternal(const DocumentReferenceInternal& other);
  DocumentReferenceInternal(DocumentReferenceInternal&& other) noexcept;
  ~DocumentReferenceInternal() override = default;
  DocumentReferenceInternal& operator=(const DocumentReferenceInternal& other) =
      delete;
  DocumentReferenceInternal& operator=(
      DocumentReferenceInternal&& other) noexcept = delete;

  Firestore* firestore() const;
  [[nodiscard]] const std::string& id() const;
  [[nodiscard]] const std::string& path() const;
  [[nodiscard]] CollectionReference Parent() const;
  [[nodiscard]] CollectionReference Collection(
      const std::string& collection_path) const;

  Future<DocumentSnapshot> Get(Source source);
  Future<void> Set(const MapFieldValue& data, const SetOptions& options);
  Future<void> Update(const MapFieldValue& data);
  Future<void> Update(const MapFieldPathValue& data);
  Future<void> Delete();

  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::function<void(const DocumentSnapshot&, Error, const std::string&)>
          callback);
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      EventListener<DocumentSnapshot>* listener);
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::unique_ptr<EventListener<DocumentSnapshot>> listener);

 private:
  friend class FirestoreInternal;
  friend class FieldValueInternal;

  ListenerRegistration AddSnapshotListenerInternal(
      MetadataChanges metadata_changes,
      EventListener<DocumentSnapshot>* listener,
      bool passing_listener_ownership);

  static jni::Class GetClass();
  static void Initialize(jni::Loader& loader);

  PromiseFactory<AsyncFn> promises_;
  mutable std::mutex mutex_;
  mutable std::string cached_id_;
  mutable std::string cached_path_;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_DOCUMENT_REFERENCE_ANDROID_H_
