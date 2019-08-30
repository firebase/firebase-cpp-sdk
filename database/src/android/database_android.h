// Copyright 2016 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATABASE_ANDROID_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATABASE_ANDROID_H_

#include <jni.h>

#include <map>
#include <set>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/log.h"
#include "app/src/logger.h"
#include "app/src/mutex.h"
#include "app/src/util_android.h"
#include "database/src/android/database_reference_android.h"
#include "database/src/common/listener.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/listener.h"

namespace firebase {
class ReferenceCountedFutureImpl;
namespace database {
namespace internal {

// For constructing, copying or moving DatabaseReferences atomically.
extern Mutex g_database_reference_constructor_mutex;

// Used for registering global callbacks. See
// firebase::util::RegisterCallbackOnTask for context.
extern const char kApiIdentifier[];

// This is the Android implementation of Database.
class DatabaseInternal {
 public:
  explicit DatabaseInternal(App* app);

  DatabaseInternal(App* app, const char* url);

  ~DatabaseInternal();

  App* GetApp() const;

  DatabaseReference GetReference() const;

  DatabaseReference GetReference(const char* path) const;

  DatabaseReference GetReferenceFromUrl(const char* url) const;

  void GoOffline() const;

  void GoOnline() const;

  void PurgeOutstandingWrites() const;

  void SetPersistenceEnabled(bool enabled) const;

  // Set the logging verbosity.
  // kLogLevelDebug and kLogLevelVerbose are interpreted as the same level by
  // the Android implementation.
  void set_log_level(LogLevel log_level);

  // Get the logging verbosity.
  LogLevel log_level() const;

  // Convert a future result code and error code from a Java DatabaseError into
  // a C++ Error enum.
  Error ErrorFromResultAndErrorCode(util::FutureResult result_code,
                                    jint java_error_code) const;

  // Convert an error code obtained from a Java DatabaseError into an C++ Error
  // enum.
  Error ErrorFromJavaErrorCode(jint java_error_code) const;

  // Convert a Java DatabaseError instance into a C++ Error enum.
  // If error_message is not null, it will be set to the error message string.
  Error ErrorFromJavaDatabaseError(jobject java_error,
                                   std::string* error_message) const;

  // Returns the Java ValueEventListener object that you will need to add to the
  // Java Query object.
  jobject RegisterValueEventListener(const internal::QuerySpec& spec,
                                     ValueListener* listener);

  // Returns a new local reference to the Java ValueEventListener object that
  // you need to remove from the Java Query object. Remember to call
  // DeleteLocalRef on the returned reference when you are finished.
  jobject UnregisterValueEventListener(const internal::QuerySpec& spec,
                                       ValueListener* listener);

  // Returns a list of new local references to Java ValueEventListener objects
  // that you should remove from the Java Query object. Remember to call
  // DeleteLocalRef on each one when finished.
  std::vector<jobject> UnregisterAllValueEventListeners(
      const internal::QuerySpec& spec);

  // Creates a Java CppValueEventListener object that calls the given C++
  // ValueListener. Returns a global reference.
  jobject CreateJavaEventListener(ValueListener* listener);

  // Resets a Java CppEventListener's pointers to null, so it won't call any
  // native callbacks.
  void ClearJavaEventListener(jobject listener);

  // Returns the Java ChildEventListener object that you will need to add to the
  // Java Query object.
  jobject RegisterChildEventListener(const internal::QuerySpec& spec,
                                     ChildListener* listener);

  // Returns a new local reference to the Java ChildEventListener object that
  // you need to remove from the Java Query object. Remember to call
  // DeleteLocalRef on the returned reference when you are finished.
  jobject UnregisterChildEventListener(const internal::QuerySpec& spec,
                                       ChildListener* listener);

  // Creates a Java CppChildEventListener object that calls the given C++
  // ChildListener. Returns a global reference.
  jobject CreateJavaEventListener(ChildListener* listener);

  // Returns a list of new local references to Java ChildEventListener objects
  // that you should remove from the Java Query object. Remember to call
  // DeleteLocalRef on each one when finished.
  std::vector<jobject> UnregisterAllChildEventListeners(
      const internal::QuerySpec& spec);

  // Track a transient listener. If the database is deleted before the listener
  // finishes, it should discard its pointers.
  void AddSingleValueListener(jobject listener) {
    MutexLock lock(listener_mutex_);
    auto found = java_single_value_listeners_.find(listener);
    if (found == java_single_value_listeners_.end()) {
      java_single_value_listeners_.insert(listener);
    }
  }
  // Finish tracking a transient listener. If the database is deleted before the
  // listener finishes, it should discard its pointers.
  void RemoveSingleValueListener(jobject listener) {
    MutexLock lock(listener_mutex_);
    auto found = java_single_value_listeners_.find(listener);
    if (found != java_single_value_listeners_.end()) {
      java_single_value_listeners_.erase(found);
    }
  }

  // Creates a transaction handler. Returns a global reference to the Java
  // Transaction.Handler object you can pass to
  // DatabaseReference.runTransaction(). This class will keep track of all
  // pending transaction handlers, and clean up any outstanding ones in the
  // destructor.
  jobject CreateJavaTransactionHandler(TransactionData* transaction_fn);

  // Removes a transaction handler, freeing the global reference and removing it
  // from the cleanup list.
  void DeleteJavaTransactionHandler(jobject transaction_handler_global);

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

  // When this is deleted, it will clean up all DatabaseReferences,
  // DataSnapshots, and other such objects.
  CleanupNotifier& cleanup() { return cleanup_; }

  // The url that was passed to the constructor.
  const std::string& constructor_url() const { return constructor_url_; }

  Logger* logger() { return &logger_; }

 private:
  static bool Initialize(App* app);
  static void ReleaseClasses(App* app);
  static void Terminate(App* app);

  // Initialize classes loaded from embedded files.
  static bool InitializeEmbeddedClasses(App* app);

  static Mutex init_mutex_;
  static int initialize_count_;
  static std::map<jint, Error>* java_error_to_cpp_;
  App* app_;
  // Java database global ref.
  jobject obj_;

  // For registering listeners.
  Mutex listener_mutex_;
  // Listeners indexed by QuerySpec.
  ListenerCollection<ValueListener> value_listeners_by_query_;
  ListenerCollection<ChildListener> child_listeners_by_query_;
  // Listeners active in Java. If Database is destroyed, these need to
  // be cleaned up.
  std::map<ValueListener*, jobject> java_value_listener_lookup_;
  std::map<ChildListener*, jobject> java_child_listener_lookup_;
  std::set<jobject> java_single_value_listeners_;

  // For registering transaction handlers.
  Mutex transaction_mutex_;
  std::set<jobject> java_transaction_handlers_;

  FutureManager future_manager_;

  CleanupNotifier cleanup_;

  // The url passed to the constructor (or "" if none was passed).
  // We keep it so that we can find the database in our cache.
  std::string constructor_url_;

  Logger logger_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATABASE_ANDROID_H_
