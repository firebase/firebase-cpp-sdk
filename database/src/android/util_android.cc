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

#include "database/src/android/util_android.h"

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/log.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "database/src/android/data_snapshot_android.h"
#include "database/src/android/database_android.h"
#include "database/src/android/mutable_data_android.h"
#include "database/src/include/firebase/database/listener.h"
#include "database/src/include/firebase/database/mutable_data.h"

namespace firebase {
namespace database {
namespace internal {

void Callbacks::ChildListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                               jlong db_ptr, jlong listener_ptr,
                                               jobject database_error) {
  (void)env;
  (void)clazz;
  if (db_ptr == 0 || listener_ptr == 0) return;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
  std::string error_msg;
  Error error_code = db->ErrorFromJavaDatabaseError(database_error, &error_msg);
  listener->OnCancelled(error_code, error_msg.c_str());
}

void Callbacks::ChildListenerNativeOnChildAdded(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child_name) {
  (void)clazz;
  if (db_ptr == 0 || listener_ptr == 0) return;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
  std::string prev_key =
      previous_child_name ? util::JStringToString(env, previous_child_name)
                          : std::string();
  listener->OnChildAdded(
      DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
      previous_child_name ? prev_key.c_str() : nullptr);
}

void Callbacks::ChildListenerNativeOnChildChanged(JNIEnv* env, jclass clazz,
                                                  jlong db_ptr,
                                                  jlong listener_ptr,
                                                  jobject data_snapshot,
                                                  jobject previous_child_name) {
  (void)clazz;
  if (db_ptr == 0 || listener_ptr == 0) return;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
  std::string prev_key =
      previous_child_name ? util::JStringToString(env, previous_child_name)
                          : std::string();
  listener->OnChildChanged(
      DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
      previous_child_name ? prev_key.c_str() : nullptr);
}

void Callbacks::ChildListenerNativeOnChildMoved(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child_name) {
  (void)clazz;
  if (db_ptr == 0 || listener_ptr == 0) return;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
  std::string prev_key =
      previous_child_name ? util::JStringToString(env, previous_child_name)
                          : std::string();
  listener->OnChildMoved(
      DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
      previous_child_name ? prev_key.c_str() : nullptr);
}

void Callbacks::ChildListenerNativeOnChildRemoved(JNIEnv* env, jclass clazz,
                                                  jlong db_ptr,
                                                  jlong listener_ptr,
                                                  jobject data_snapshot) {
  (void)env;
  (void)clazz;
  if (db_ptr == 0 || listener_ptr == 0) return;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
  listener->OnChildRemoved(
      DataSnapshot(new DataSnapshotInternal(db, data_snapshot)));
}

void Callbacks::ValueListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                               jlong db_ptr, jlong listener_ptr,
                                               jobject database_error) {
  (void)env;
  (void)clazz;
  if (db_ptr == 0 || listener_ptr == 0) return;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* listener = reinterpret_cast<ValueListener*>(listener_ptr);
  std::string error_msg;
  Error error_code = db->ErrorFromJavaDatabaseError(database_error, &error_msg);
  listener->OnCancelled(error_code, error_msg.c_str());
}

void Callbacks::ValueListenerNativeOnDataChange(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot) {
  (void)env;
  (void)clazz;
  if (db_ptr == 0 || listener_ptr == 0) return;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* listener = reinterpret_cast<ValueListener*>(listener_ptr);
  listener->OnValueChanged(
      DataSnapshot(new DataSnapshotInternal(db, data_snapshot)));
}

jobject Callbacks::TransactionHandlerDoTransaction(JNIEnv* env, jclass clazz,
                                                   jlong db_ptr,
                                                   jlong transaction_data_ptr,
                                                   jobject java_mutable_data) {
  (void)env;
  (void)clazz;
  if (db_ptr == 0 || transaction_data_ptr == 0) return nullptr;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* data = reinterpret_cast<TransactionData*>(transaction_data_ptr);
  MutableData mutable_data = MutableDataInternal::MakeMutableData(
      new MutableDataInternal(db, java_mutable_data));
  return data->transaction(&mutable_data, data->context) ==
                 kTransactionResultSuccess
             ? java_mutable_data
             : nullptr;
}

void Callbacks::TransactionHandlerOnComplete(
    JNIEnv* env, jclass clazz, jlong db_ptr, jlong transaction_data_ptr,
    jobject database_error, jboolean was_committed, jobject data_snapshot) {
  (void)clazz;
  if (db_ptr == 0 || transaction_data_ptr == 0) return;
  auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
  auto* data = reinterpret_cast<TransactionData*>(transaction_data_ptr);
  auto* future = data->future;
  auto handle = data->handle;
  jobject java_handler_global = data->java_handler;
  if (was_committed || database_error == nullptr) {
    jobject snap_global =
        data_snapshot ? env->NewGlobalRef(data_snapshot) : nullptr;
    db->DeleteJavaTransactionHandler(java_handler_global);
    Error err = was_committed ? kErrorNone : kErrorTransactionAbortedByUser;
    const char* msg =
        was_committed
            ? ""
            : "The transaction was aborted, because the transaction function "
              "returned kTransactionResultAbort.";
    future->Complete<DataSnapshot>(
        handle, err, msg, [env, db, snap_global](DataSnapshot* result) {
          if (snap_global != nullptr) {
            *result = DataSnapshot(new DataSnapshotInternal(db, snap_global));
            env->DeleteGlobalRef(snap_global);
          } else {
            *result = DataSnapshot(nullptr);
          }
        });
  } else {
    std::string error_message;
    Error error = db->ErrorFromJavaDatabaseError(database_error, &error_message);
    db->DeleteJavaTransactionHandler(java_handler_global);
    future->Complete(handle, error, error_message.c_str());
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
