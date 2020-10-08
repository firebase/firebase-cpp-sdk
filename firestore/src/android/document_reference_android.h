#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_REFERENCE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_REFERENCE_ANDROID_H_

#include <string>

#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/promise_factory_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class Firestore;

// This is the Android implementation of DocumentReference.
class DocumentReferenceInternal : public Wrapper {
 public:
  // Each API of DocumentReference that returns a Future needs to define an enum
  // value here. For example, a Future-returning method Foo() relies on the enum
  // value kFoo. The enum values are used to identify and manage Future in the
  // Firestore Future manager.
  enum class AsyncFn {
    kGet = 0,
    kSet,
    kUpdate,
    kDelete,
    kCount,  // Must be the last enum value.
  };

  /**
   * Creates a C++ `DocumentReference` from a Java `DocumentReference` object.
   */
  static DocumentReference Create(jni::Env& env, const jni::Object& reference);

  DocumentReferenceInternal(FirestoreInternal* firestore,
                            const jni::Object& object)
      : Wrapper(firestore, object), promises_(firestore) {}

  /** Gets the Firestore instance associated with this document reference. */
  Firestore* firestore();

  /**
   * Gets the document-id of this document.
   *
   * @return A reference to the document-id of this document.
   */
  const std::string& id() const;

  /**
   * Gets the path of this document (relative to the root of the database) as a
   * slash-separated string.
   *
   * @return A reference to the path of this document.
   */
  const std::string& path() const;

  /**
   * Gets a CollectionReference to the collection that contains this document.
   *
   * @return The CollectionReference that contains this document.
   */
  CollectionReference Parent() const;

  /**
   * Gets a CollectionReference instance that refers to the subcollection at the
   * specified path relative to this document.
   *
   * @param collectionPath A slash-separated relative path to a subcollection.
   * @return The CollectionReference instance.
   */
  CollectionReference Collection(const std::string& collection_path);

  /**
   * Reads this document.
   *
   * By default, Get() attempts to provide up-to-date data when possible by
   * waiting for data from the server, but it may return cached data or fail if
   * you are offline and the server cannot be reached. This behavior can be
   * altered via the source parameter.
   *
   * @param source A value to configure the get behavior.
   *
   * @return A Future that will be resolved with the contents of the document.
   */
  Future<DocumentSnapshot> Get(Source source);

  /**
   * Writes to this document.
   *
   * If the document does not yet exist, it will be created. If you pass
   * SetOptions, the provided data can be merged into an existing document.
   *
   * @param data A map of the fields and values for the document.
   * @param options An object to configure the set behavior.
   *
   * @return A Future that will be resolved when the write finishes.
   */
  Future<void> Set(const MapFieldValue& data, const SetOptions& options);

  /**
   * Updates fields in this document.
   *
   * If no document exists yet, the update will fail.
   *
   * @param data A map of field / value pairs to update. Fields can contain
   * dots to reference nested fields within the document.
   *
   * @return A Future that will be resolved when the write finishes.
   */
  Future<void> Update(const MapFieldValue& data);

  /**
   * Updates fields in this document.
   *
   * If no document exists yet, the update will fail.
   *
   * @param data A map from FieldPath to FieldValue to update.
   *
   * @return A Future that will be resolved when the write finishes.
   */
  Future<void> Update(const MapFieldPathValue& data);

  /**
   * Removes this document.
   *
   * @return A Future that will be resolved when the delete completes.
   */
  Future<void> Delete();

#if defined(FIREBASE_USE_STD_FUNCTION)
  /**
   * @brief Starts listening to the document referenced by this
   * DocumentReference.
   *
   * @param[in] metadata_changes Indicates whether metadata-only changes (i.e.
   * only DocumentSnapshot.getMetadata() changed) should trigger snapshot
   * events.
   * @param[in] callback function or lambda to call. When this function is
   * called, snapshot value is valid if and only if error is Error::kErrorOk.
   *
   * @return A registration object that can be used to remove the listener.
   *
   * @note This method is not available when using the STLPort C++ runtime
   * library.
   */
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::function<void(const DocumentSnapshot&, Error, const std::string&)>
          callback);
#endif  // defined(FIREBASE_USE_STD_FUNCTION)

  /**
   * Starts listening to the document referenced by this DocumentReference.
   *
   * @param[in] metadata_changes Indicates whether metadata-only changes (i.e.
   * only DocumentSnapshot.getMetadata() changed) should trigger snapshot
   * events.
   * @param[in] listener The event listener that will be called with the
   * snapshots, which must remain in memory until you remove the listener from
   * this DocumentReference. (Ownership is not transferred; you are responsible
   * for making sure that listener is valid as long as this DocumentReference is
   * valid and the listener is registered.)
   * @param[in] passing_listener_ownership Whether to pass the ownership of the
   * listener.
   *
   * @return A registration object that can be used to remove the listener.
   */
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      EventListener<DocumentSnapshot>* listener,
      bool passing_listener_ownership = false);

  /** @brief Gets the class object of Java DocumentReference class. */
  static jni::Class GetClass();

 private:
  friend class FirestoreInternal;

  static void Initialize(jni::Loader& loader);

  PromiseFactory<AsyncFn> promises_;

  // Below are cached call results.
  mutable std::string cached_id_;
  mutable std::string cached_path_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_REFERENCE_ANDROID_H_
