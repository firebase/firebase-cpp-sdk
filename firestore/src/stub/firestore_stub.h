#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_FIRESTORE_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_FIRESTORE_STUB_H_

#include "app/src/cleanup_notifier.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/include/firebase/firestore.h"

namespace firebase {
namespace firestore {

class FirestoreInternal;

// There is no internal type for stub (yet). We define these stubs to suppress
// incomplete type error for now.
class Stub {
 public:
  FirestoreInternal* firestore_internal() { return nullptr; }
};

// This is the stub implementation of Firestore.
class FirestoreInternal {
 public:
  using ApiType = Firestore;
  explicit FirestoreInternal(App* app) : app_(app) {}

  ~FirestoreInternal() {}

  App* app() const { return app_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

  // Default CleanupNotifier as required by the shared code; nothing more.
  CleanupNotifier& cleanup() { return cleanup_; }

  // Returns a null Collection.
  CollectionReference Collection(const char* collection_path) const {
    return CollectionReference{};
  }

  // Returns a null Document.
  DocumentReference Document(const char* document_path) const {
    return DocumentReference{};
  }

  // Returns a null Query.
  Query CollectionGroup(const char* collection_id) const { return Query{}; }

  // Gets the settings class member. Has no effect to the stub yet.
  Settings settings() const { return settings_; }

  // Sets the settings class member. Has no effect to the stub yet.
  void set_settings(Settings settings) { settings_ = settings; }

  WriteBatch batch() const { return WriteBatch{}; }

  // Runs transaction atomically.
  Future<void> RunTransaction(TransactionFunction* update) {
    return FailedFuture<void>();
  }

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
  Future<void> RunTransaction(
      std::function<Error(Transaction&, std::string&)> update) {
    return FailedFuture<void>();
  }
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

  // Disables network and gets anything from cache instead of server.
  Future<void> DisableNetwork() { return FailedFuture<void>(); }

  // Re-enables network after a prior call to DisableNetwork().
  Future<void> EnableNetwork() { return FailedFuture<void>(); }

  Future<void> Terminate() { return FailedFuture<void>(); }

  Future<void> WaitForPendingWrites() { return FailedFuture<void>(); }

  Future<void> ClearPersistence() { return FailedFuture<void>(); }

  ListenerRegistration AddSnapshotsInSyncListener(
      EventListener<void>* listener) {
    return ListenerRegistration{};
  }
#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
  ListenerRegistration AddSnapshotsInSyncListener(
      // NOLINTNEXTLINE (performance-unnecessary-value-param)
      std::function<void()> callback) {
    return ListenerRegistration{};
  }
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

  static void set_log_level(LogLevel level);

  void UnregisterListenerRegistration(
      ListenerRegistrationInternal* registration) {}

  void ClearListeners() {}

  // The following builders are test helpers to avoid expose the details into
  // public header.
  template <typename InternalType>
  static typename InternalType::ApiType Wrap(InternalType* internal) {
    return typename InternalType::ApiType(internal);
  }

  template <typename InternalType>
  static InternalType* Internal(typename InternalType::ApiType& value) {
    // Cast is required for the case when the InternalType has hierachy e.g.
    // CollectionReferenceInternal vs QueryInternal (check their implementation
    // for more details).
    return static_cast<InternalType*>(value.internal_);
  }

  void set_firestore_public(Firestore*) {}

  static void SetClientLanguage(const std::string& language_token) {}

 private:
  CleanupNotifier cleanup_;
  App* app_;
  Settings settings_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_FIRESTORE_STUB_H_
