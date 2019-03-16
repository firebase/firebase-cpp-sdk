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

namespace {

// Converts an std::vector<Variant> to a java.util.List<java.lang.Object>.
jobject StdVectorToJavaList(JNIEnv* env, const std::vector<Variant>& vector) {
  jobject java_list = env->NewObject(
      util::array_list::GetClass(),
      util::array_list::GetMethodId(util::array_list::kConstructorWithSize),
      vector.size());

  jmethodID add_method = util::array_list::GetMethodId(util::array_list::kAdd);
  for (int i = 0; i < vector.size(); i++) {
    jobject element = VariantToJavaObject(env, vector[i]);
    env->CallBooleanMethod(java_list, add_method, element);
    env->DeleteLocalRef(element);
  }
  return java_list;
}

// Converts an std::map<Variant, Variant> to a java.util.Map<java.lang.String,
// java.lang.Object>.  The jobject returned will be a local reference, so if
// you'll be using it from other threads, be sure to create a global reference
// and delete the local.
jobject StdMapToJavaMap(JNIEnv* env, const std::map<Variant, Variant>& map) {
  jobject java_map =
      env->NewObject(util::hash_map::GetClass(),
                     util::hash_map::GetMethodId(util::hash_map::kConstructor));

  jmethodID put_method = util::map::GetMethodId(util::map::kPut);
  for (auto i = map.begin(); i != map.end(); ++i) {
    // Force the key into a string.
    jobject key = VariantToJavaObject(env, i->first.AsString());
    jobject value = VariantToJavaObject(env, i->second);
    jobject previous = env->CallObjectMethod(java_map, put_method, key, value);
    if (previous) env->DeleteLocalRef(previous);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(key);
  }
  return java_map;
}

// Converts a java.util.List<java.lang.Object> to a std::vector<Variant>.
void JavaListToStdVector(JNIEnv* env, jobject list,
                         std::vector<Variant>* vector) {
  int size =
      env->CallIntMethod(list, util::list::GetMethodId(util::list::kSize));
  vector->clear();
  vector->reserve(size);
  for (int i = 0; i < size; i++) {
    jobject element = env->CallObjectMethod(
        list, util::list::GetMethodId(util::list::kGet), i);
    vector->push_back(JavaObjectToVariant(env, element));
    env->DeleteLocalRef(element);
  }
}

// Converts a java.util.Map<java.lang.String, java.lang.Object> to a
// std::map<Variant, Variant>.
void JavaMapToStdMap(JNIEnv* env, jobject java_map,
                     std::map<Variant, Variant>* cpp_map) {
  cpp_map->clear();
  // Set<Object> key_set = java_map.keySet();
  jobject key_set = env->CallObjectMethod(
      java_map, util::map::GetMethodId(util::map::kKeySet));
  // Iterator iter = key_set.iterator();
  jobject iter = env->CallObjectMethod(
      key_set, util::set::GetMethodId(util::set::kIterator));
  // while (iter.hasNext())
  while (env->CallBooleanMethod(
      iter, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    // Object key = iter.next();
    jobject key_object = env->CallObjectMethod(
        iter, util::iterator::GetMethodId(util::iterator::kNext));
    Variant key = JavaObjectToVariant(env, key_object);
    // Object value = java_map.get(key);
    jobject value_object = env->CallObjectMethod(
        java_map, util::map::GetMethodId(util::map::kGet), key_object);
    Variant value = JavaObjectToVariant(env, value_object);
    cpp_map->insert(std::make_pair(key, value));
    env->DeleteLocalRef(value_object);
    env->DeleteLocalRef(key_object);
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}

}  // namespace

// Converts a Variant to a java.lang.Object, returned as a local reference.
jobject VariantToJavaObject(JNIEnv* env, const Variant& variant) {
  if (variant.is_null()) {
    return nullptr;
  } else if (variant.is_int64()) {
    return env->NewObject(
        util::long_class::GetClass(),
        util::long_class::GetMethodId(util::long_class::kConstructor),
        variant.int64_value());
  } else if (variant.is_double()) {
    return env->NewObject(
        util::double_class::GetClass(),
        util::double_class::GetMethodId(util::double_class::kConstructor),
        variant.double_value());
  } else if (variant.is_bool()) {
    return env->NewObject(
        util::boolean_class::GetClass(),
        util::boolean_class::GetMethodId(util::boolean_class::kConstructor),
        variant.bool_value());
  } else if (variant.is_string()) {
    return env->NewStringUTF(variant.string_value());
  } else if (variant.is_vector()) {
    return StdVectorToJavaList(env, variant.vector());
  } else if (variant.is_map()) {
    return StdMapToJavaMap(env, variant.map());
  }
  LogWarning("Unknown Variant type, cannot convert into Java object.");
  return nullptr;
}

// Converts a java.lang.Object into a Variant.
Variant JavaObjectToVariant(JNIEnv* env, jobject obj) {
  if (obj == nullptr) {
    return Variant::Null();
  } else if (env->IsInstanceOf(obj, util::long_class::GetClass())) {
    return Variant::FromInt64(env->CallLongMethod(
        obj, util::long_class::GetMethodId(util::long_class::kValue)));
  } else if (env->IsInstanceOf(obj, util::double_class::GetClass())) {
    return Variant::FromDouble(env->CallDoubleMethod(
        obj, util::double_class::GetMethodId(util::double_class::kValue)));
  } else if (env->IsInstanceOf(obj, util::boolean_class::GetClass())) {
    return Variant::FromBool(env->CallBooleanMethod(
        obj, util::boolean_class::GetMethodId(util::boolean_class::kValue)));
  } else if (env->IsInstanceOf(obj, util::string::GetClass())) {
    return Variant::FromMutableString(util::JStringToString(env, obj));
  } else if (env->IsInstanceOf(obj, util::list::GetClass())) {
    Variant v = Variant::EmptyVector();
    JavaListToStdVector(env, obj, &v.vector());
    return v;
  } else if (env->IsInstanceOf(obj, util::map::GetClass())) {
    Variant v = Variant::EmptyMap();
    JavaMapToStdMap(env, obj, &v.map());
    return v;
  }
  LogWarning("Unknown Java object type, cannot convert into Variant.");
  return Variant::Null();
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
