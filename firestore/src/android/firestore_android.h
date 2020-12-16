#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIRESTORE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIRESTORE_ANDROID_H_

#include <cstdint>
#include <unordered_set>

#if defined(FIREBASE_USE_STD_FUNCTION)
#include <functional>
#endif

#include "app/memory/unique_ptr.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {

class Firestore;
class ListenerRegistrationInternal;
class Transaction;
class TransactionFunction;
class WriteBatch;

template <typename EnumT>
class PromiseFactory;

// Used for registering global callbacks. See
// firebase::util::RegisterCallbackOnTask for context.
extern const char kApiIdentifier[];

// This is the Android implementation of Firestore. Cannot inherit WrapperFuture
// as a valid FirestoreInternal is required to construct a WrapperFuture. So we
// will implement the Future APIs on the fly.
class FirestoreInternal {
 public:
  // Each API of Firestore that returns a Future needs to define an enum
  // value here. For example, a Future-returning method Foo() relies on the enum
  // value kFoo. The enum values are used to identify and manage Future in the
  // Firestore Future manager.
  enum class AsyncFn {
    kEnableNetwork = 0,
    kDisableNetwork,
    kRunTransaction,
    kTerminate,
    kWaitForPendingWrites,
    kClearPersistence,
    kCount,  // Must be the last enum value.
  };

  // Note: call `set_firestore_public` immediately after construction.
  explicit FirestoreInternal(App* app);
  ~FirestoreInternal();

  App* app() const { return app_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

  // Manages all Future objects returned from Firestore API.
  FutureManager& future_manager() { return future_manager_; }

  // When this is deleted, it will clean up all DatabaseReferences,
  // DataSnapshots, and other such objects.
  CleanupNotifier& cleanup() { return cleanup_; }

  // Returns a CollectionReference that refers to the specific path.
  CollectionReference Collection(const char* collection_path) const;

  // Returns a DocumentReference that refers to the specific path.
  DocumentReference Document(const char* document_path) const;

  // Returns a Collection Group Query that refers to the specified collection.
  Query CollectionGroup(const char* collection_id) const;

  // Returns the settings used by this Firestore object.
  Settings settings() const;

  // Sets any custom settings used to configure this Firestore object.
  void set_settings(Settings settings);

  WriteBatch batch() const;

  // Runs transaction atomically.
  Future<void> RunTransaction(TransactionFunction* update,
                              bool is_lambda = false);
#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
  Future<void> RunTransaction(
      std::function<Error(Transaction&, std::string&)> update);
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

  // Disables network and gets anything from cache instead of server.
  Future<void> DisableNetwork();

  // Re-enables network after a prior call to DisableNetwork().
  Future<void> EnableNetwork();

  Future<void> Terminate();

  Future<void> WaitForPendingWrites();

  Future<void> ClearPersistence();

  ListenerRegistration AddSnapshotsInSyncListener(
      EventListener<void>* listener, bool passing_listener_ownership = false);

#if defined(FIREBASE_USE_STD_FUNCTION)
  ListenerRegistration AddSnapshotsInSyncListener(
      std::function<void()> callback);
#endif  // defined(FIREBASE_USE_STD_FUNCTION)

  // Manages the ListenerRegistrationInternal objects.
  void RegisterListenerRegistration(ListenerRegistrationInternal* registration);
  void UnregisterListenerRegistration(
      ListenerRegistrationInternal* registration);
  void ClearListeners();

  static jni::Env GetEnv();

  CollectionReference NewCollectionReference(
      jni::Env& env, const jni::Object& reference) const;
  DocumentReference NewDocumentReference(jni::Env& env,
                                         const jni::Object& reference) const;
  DocumentSnapshot NewDocumentSnapshot(jni::Env& env,
                                       const jni::Object& snapshot) const;
  Query NewQuery(jni::Env& env, const jni::Object& query) const;
  QuerySnapshot NewQuerySnapshot(jni::Env& env,
                                 const jni::Object& snapshot) const;

  void set_firestore_public(Firestore* firestore_public) {
    firestore_public_ = firestore_public;
  }

  Firestore* firestore_public() { return firestore_public_; }
  const Firestore* firestore_public() const { return firestore_public_; }

  /**
   * Finds the FirestoreInternal instance for the given Java Firestore instance.
   */
  static FirestoreInternal* RecoverFirestore(jni::Env& env,
                                             const jni::Object& java_firestore);

  const jni::Global<jni::Object>& user_callback_executor() const {
    return user_callback_executor_;
  }

  static void SetClientLanguage(const std::string& language_token);

 private:
  friend class FirestoreIntegrationTest;

  FirestoreInternal* mutable_this() const {
    return const_cast<FirestoreInternal*>(this);
  }

  void ShutdownUserCallbackExecutor(jni::Env& env);

  static bool Initialize(App* app);
  static void ReleaseClasses(App* app);
  static void Terminate(App* app);

  static Mutex init_mutex_;
  static int initialize_count_;
  static jni::Loader* loader_;

  jni::Global<jni::Object> user_callback_executor_;

  App* app_ = nullptr;
  Firestore* firestore_public_ = nullptr;
  // Java Firestore global ref.
  jni::Global<jni::Object> obj_;

  Mutex listener_registration_mutex_;  // For registering listener-registrations
#if defined(_STLPORT_VERSION)
  struct ListenerRegistrationInternalPointerHash {
    std::size_t operator()(const ListenerRegistrationInternal* value) const {
      uintptr_t address = reinterpret_cast<uintptr_t>(value);
      // Simply returning address is a bad hash. Due to alignment, the right 3
      // bits could all be 0. There could be better hash for pointers. We use
      // this for the STLPort for now.
      return address + (address >> 3);
    }
  };
  std::tr1::unordered_set<ListenerRegistrationInternal*,
                          ListenerRegistrationInternalPointerHash>
      listener_registrations_;
#else   //  defined(_STLPORT_VERSION)
  std::unordered_set<ListenerRegistrationInternal*> listener_registrations_;
#endif  //  defined(_STLPORT_VERSION)

  FutureManager future_manager_;
  UniquePtr<PromiseFactory<AsyncFn>> promises_;

  CleanupNotifier cleanup_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIRESTORE_ANDROID_H_
