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

// Converts a Variant to a java.lang.Object, returned as a local reference.
jobject VariantToJavaObject(JNIEnv* env, const Variant& variant) {
  return ::firebase::util::VariantToJavaObject(env, variant);
}

// Converts a java.lang.Object into a Variant.
Variant JavaObjectToVariant(JNIEnv* env, jobject obj) {
  return ::firebase::util::JavaObjectToVariant(env, obj);
}

void Callbacks::ChildListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                               jlong db_ptr, jlong listener_ptr,
                                               jobject database_error) {
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    std::string error_msg;
    Error error_code =
        db->ErrorFromJavaDatabaseError(database_error, &error_msg);
    listener->OnCancelled(error_code, error_msg.c_str());
  }
}

void Callbacks::ChildListenerNativeOnChildAdded(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child_name) {
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    listener->OnChildAdded(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name
            ? util::JStringToString(env, previous_child_name).c_str()
            : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildChanged(JNIEnv* env, jclass clazz,
                                                  jlong db_ptr,
                                                  jlong listener_ptr,
                                                  jobject data_snapshot,
                                                  jobject previous_child_name) {
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    listener->OnChildChanged(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name
            ? util::JStringToString(env, previous_child_name).c_str()
            : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildMoved(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child_name) {
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    listener->OnChildMoved(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name
            ? util::JStringToString(env, previous_child_name).c_str()
            : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildRemoved(JNIEnv* env, jclass clazz,
                                                  jlong db_ptr,
                                                  jlong listener_ptr,
                                                  jobject data_snapshot) {
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    listener->OnChildRemoved(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)));
  }
}

void Callbacks::ValueListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                               jlong db_ptr, jlong listener_ptr,
                                               jobject database_error) {
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ValueListener* listener = reinterpret_cast<ValueListener*>(listener_ptr);
    std::string error_msg;
    Error error_code =
        db->ErrorFromJavaDatabaseError(database_error, &error_msg);
    listener->OnCancelled(error_code, error_msg.c_str());
  }
}

void Callbacks::ValueListenerNativeOnDataChange(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child_name) {
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ValueListener* listener = reinterpret_cast<ValueListener*>(listener_ptr);
    listener->OnValueChanged(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)));
  }
}

jobject Callbacks::TransactionHandlerDoTransaction(JNIEnv* env, jclass clazz,
                                                   jlong db_ptr,
                                                   jlong transaction_data_ptr,
                                                   jobject java_mutable_data) {
  if (db_ptr != 0 && transaction_data_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    TransactionData* data =
        reinterpret_cast<TransactionData*>(transaction_data_ptr);
    DoTransactionWithContext user_func = data->transaction;
    MutableData mutable_data = MutableDataInternal::MakeMutableData(
        new MutableDataInternal(db, java_mutable_data));
    TransactionResult result = user_func(&mutable_data, data->context);
    if (result == kTransactionResultSuccess) {
      return java_mutable_data;
    } else {
      return nullptr;
    }
  }
  return nullptr;
}

void Callbacks::TransactionHandlerOnComplete(
    JNIEnv* env, jclass clazz, jlong db_ptr, jlong transaction_data_ptr,
    jobject database_error, jboolean was_committed, jobject data_snapshot) {
  if (db_ptr != 0 && transaction_data_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    TransactionData* data =
        reinterpret_cast<TransactionData*>(transaction_data_ptr);
    if (was_committed) {
      // Transaction success!
      jobject data_snapshot_global = env->NewGlobalRef(data_snapshot);
      data->future->Complete<DataSnapshot>(
          data->handle, kErrorNone, "",
          [db, data_snapshot_global](DataSnapshot* result) {
            *result = DataSnapshot(
                new DataSnapshotInternal(db, data_snapshot_global));
            db->GetApp()->GetJNIEnv()->DeleteGlobalRef(data_snapshot_global);
          });
    } else if (database_error == nullptr) {
      jobject data_snapshot_global = env->NewGlobalRef(data_snapshot);
      data->future->Complete<DataSnapshot>(
          data->handle, kErrorTransactionAbortedByUser,
          "The transaction was aborted, because the transaction function "
          "returned kTransactionResultAbort.",
          [db, data_snapshot_global](DataSnapshot* result) {
            *result = DataSnapshot(
                new DataSnapshotInternal(db, data_snapshot_global));
            db->GetApp()->GetJNIEnv()->DeleteGlobalRef(data_snapshot_global);
          });
    } else {
      // Future error.
      std::string error_message;
      Error error =
          db->ErrorFromJavaDatabaseError(database_error, &error_message);
      data->future->Complete(data->handle, error, error_message.c_str());
    }
    jobject java_handler_global = data->java_handler;
    db->DeleteJavaTransactionHandler(java_handler_global);  // deletes `data`.
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
