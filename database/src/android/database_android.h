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

#ifndef FIREBASE_DATABASE_SRC_ANDROID_DATABASE_ANDROID_H_
#define FIREBASE_DATABASE_SRC_ANDROID_DATABASE_ANDROID_H_

#include <jni.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/src/app_android.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/log.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "app/src/logger.h"
#include "app/src/safe_reference.h"
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

extern Mutex g_database_reference_constructor_mutex;

#ifndef FIREBASE_DATABASE_ANDROID_CALLBACKS_DEFINED_
#define FIREBASE_DATABASE_ANDROID_CALLBACKS_DEFINED_
class Callbacks {
 public:
  static jobject TransactionHandlerDoTransaction(JNIEnv* env, jclass clazz,
                                                 jlong db_ptr,
                                                 jlong transaction_ptr,
                                                 jobject mutable_data);
  static void TransactionHandlerOnComplete(JNIEnv* env, jclass clazz,
                                           jlong db_ptr, jlong listener_ptr,
                                           jobject database_error,
                                           jboolean was_committed,
                                           jobject data_snapshot);
  static void ChildListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                             jlong db_ptr, jlong listener_ptr,
                                             jobject database_error);
  static void ChildListenerNativeOnChildAdded(JNIEnv* env, jclass clazz,
                                              jlong db_ptr, jlong listener_ptr,
                                              jobject data_snapshot,
                                              jobject previous_child);
  static void ChildListenerNativeOnChildChanged(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child);
  static void ChildListenerNativeOnChildMoved(JNIEnv* env, jclass clazz,
                                              jlong db_ptr, jlong listener_ptr,
                                              jobject data_snapshot,
                                              jobject previous_child);
  static void ChildListenerNativeOnChildRemoved(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot);
  static void ValueListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                             jlong db_ptr, jlong listener_ptr,
                                             jobject database_error);
  static void ValueListenerNativeOnDataChange(JNIEnv* env, jclass clazz,
                                              jlong db_ptr, jlong listener_ptr,
                                              jobject data_snapshot);
};
#endif  // FIREBASE_DATABASE_ANDROID_CALLBACKS_DEFINED_

class DatabaseInternal {
 public:
  explicit DatabaseInternal(App* app);
  DatabaseInternal(App* app, const char* url);
  ~DatabaseInternal();

  App* GetApp() const { return app_; }
  DatabaseReference GetReference() const;
  DatabaseReference GetReference(const char* path) const;
  DatabaseReference GetReferenceFromUrl(const char* url) const;
  void GoOffline() const;
  void GoOnline() const;
  void PurgeOutstandingWrites() const;
  void SetPersistenceEnabled(bool enabled) const;
  void set_log_level(LogLevel log_level);
  LogLevel log_level() const { return logger_.GetLogLevel(); }

  Error ErrorFromResultAndErrorCode(util::FutureResult result_code,
                                    jint java_error_code) const;
  Error ErrorFromJavaErrorCode(jint java_error_code) const;
  Error ErrorFromJavaDatabaseError(jobject java_error,
                                   std::string* error_message) const;
  Error ErrorFromJavaThrowable(jobject throwable,
                               std::string* error_message) const;

  void RegisterTaskCallback(JNIEnv* env, jobject task,
                            SafeFutureHandle<void> handle,
                            ReferenceCountedFutureImpl* future_impl,
                            const char* error_msg);

  jobject RegisterValueEventListener(const internal::QuerySpec& spec,
                                     ValueListener* listener);
  jobject UnregisterValueEventListener(const internal::QuerySpec& spec,
                                       ValueListener* listener);
  std::vector<jobject> UnregisterAllValueEventListeners(
      const internal::QuerySpec& spec);
  jobject CreateJavaEventListener(ValueListener* listener);
  void ClearJavaEventListener(jobject listener);

  jobject RegisterChildEventListener(const internal::QuerySpec& spec,
                                     ChildListener* listener);
  jobject UnregisterChildEventListener(const internal::QuerySpec& spec,
                                       ChildListener* listener);
  jobject CreateJavaEventListener(ChildListener* listener);
  std::vector<jobject> UnregisterAllChildEventListeners(
      const internal::QuerySpec& spec);

  void AddSingleValueListener(jobject listener) {
    if (!listener) return;
    MutexLock lock(listener_mutex_);
    java_single_value_listeners_.insert(listener);
  }
  void RemoveSingleValueListener(jobject listener) {
    if (!listener) return;
    MutexLock lock(listener_mutex_);
    java_single_value_listeners_.erase(listener);
  }
  void RegisterSingleValueListener(ValueListener* listener) {
    if (!listener) return;
    MutexLock lock(listener_mutex_);
    single_value_listeners_.insert(listener);
  }
  void UnregisterSingleValueListener(ValueListener* listener) {
    if (!listener) return;
    MutexLock lock(listener_mutex_);
    single_value_listeners_.erase(listener);
  }
  bool IsValueListenerRegistered(ValueListener* listener) {
    MutexLock lock(listener_mutex_);
    return java_value_listener_lookup_.find(listener) !=
               java_value_listener_lookup_.end() ||
           single_value_listeners_.find(listener) !=
               single_value_listeners_.end();
  }
  bool IsChildListenerRegistered(ChildListener* listener) {
    MutexLock lock(listener_mutex_);
    return java_child_listener_lookup_.find(listener) !=
           java_child_listener_lookup_.end();
  }

  Mutex& listener_mutex() { return listener_mutex_; }
  jobject CreateJavaTransactionHandler(TransactionData* transaction_fn);
  void DeleteJavaTransactionHandler(jobject transaction_handler_global);

  FutureManager& future_manager() { return future_manager_; }
  bool initialized() const { return app_ != nullptr; }
  CleanupNotifier& cleanup() { return cleanup_; }
  const std::string& constructor_url() const { return constructor_url_; }
  Logger* logger() { return &logger_; }
  const char* jni_task_id() { return jni_task_id_.c_str(); }

  typedef firebase::internal::SafeReference<DatabaseInternal> ThisRef;
  typedef firebase::internal::SafeReferenceLock<DatabaseInternal> ThisRefLock;
  ThisRef& this_ref() { return safe_this_; }

 private:
  template <typename T>
  jobject RegisterListener(const internal::QuerySpec& spec, T* listener,
                           ListenerCollection<T>* by_query,
                           std::map<T*, jobject>* lookup);
  template <typename T>
  jobject UnregisterListenerLocked(const internal::QuerySpec& spec, T* listener,
                                   ListenerCollection<T>* by_query,
                                   std::map<T*, jobject>* lookup,
                                   jobject* out_global_to_discard);
  template <typename T>
  jobject UnregisterListener(const internal::QuerySpec& spec, T* listener,
                             ListenerCollection<T>* by_query,
                             std::map<T*, jobject>* lookup);
  template <typename T>
  std::vector<jobject> UnregisterAllListeners(const internal::QuerySpec& spec,
                                              ListenerCollection<T>* by_query,
                                              std::map<T*, jobject>* lookup);

  static bool Initialize(App* app);
  static void ReleaseClasses(App* app);
  static void Terminate(App* app);
  static bool InitializeEmbeddedClasses(App* app);

  static Mutex init_mutex_;
  static int initialize_count_;
  static std::map<jint, Error>* java_error_to_cpp_;
  App* app_;
  jobject obj_;

  Mutex listener_mutex_;
  ListenerCollection<ValueListener> value_listeners_by_query_;
  ListenerCollection<ChildListener> child_listeners_by_query_;
  std::map<ValueListener*, jobject> java_value_listener_lookup_;
  std::map<ChildListener*, jobject> java_child_listener_lookup_;
  std::set<jobject> java_single_value_listeners_;
  std::set<ValueListener*> single_value_listeners_;

  Mutex transaction_mutex_;
  std::set<jobject> java_transaction_handlers_;

  FutureManager future_manager_;
  CleanupNotifier cleanup_;
  std::string constructor_url_;
  Logger logger_;
  std::string jni_task_id_;
  ThisRef safe_this_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_ANDROID_DATABASE_ANDROID_H_
