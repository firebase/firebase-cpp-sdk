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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_QUERY_ANDROID_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_QUERY_ANDROID_H_

#include <jni.h>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/common/query_spec.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/listener.h"

namespace firebase {
namespace database {
namespace internal {

class QueryInternal {
 public:
  // QueryInternal will create its own global reference to query_obj,
  // so you should delete the object you passed in after creating the
  // QueryInternal instance.
  QueryInternal(DatabaseInternal* database, jobject query_obj);

  QueryInternal(DatabaseInternal* database, jobject query_obj,
                const internal::QuerySpec& query_spec);

  QueryInternal(const QueryInternal& query);

  QueryInternal& operator=(const QueryInternal& query);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  QueryInternal(QueryInternal&& query);

  QueryInternal& operator=(QueryInternal&& query);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  virtual ~QueryInternal();

  Future<DataSnapshot> GetValue();
  Future<DataSnapshot> GetValueLastResult();

  void AddValueListener(ValueListener* listener);

  void RemoveValueListener(ValueListener* listener);

  void RemoveAllValueListeners();

  void AddChildListener(ChildListener* listener);

  void RemoveChildListener(ChildListener* listener);

  void RemoveAllChildListeners();

  // Returns a new DatabaseReferenceInternal allocated on the heap, pointing
  // to this location of the database (discarding all ordering/filters/limits).
  DatabaseReferenceInternal* GetReference();

  void SetKeepSynchronized(bool keep_sync);

  QueryInternal* OrderByChild(const char* path);
  QueryInternal* OrderByKey();
  QueryInternal* OrderByPriority();
  QueryInternal* OrderByValue();

  QueryInternal* StartAt(Variant order_value);
  QueryInternal* StartAt(Variant order_value, const char* child_key);

  QueryInternal* EndAt(Variant order_value);
  QueryInternal* EndAt(Variant order_value, const char* child_key);

  QueryInternal* EqualTo(Variant order_value);
  QueryInternal* EqualTo(Variant order_value, const char* child_key);

  QueryInternal* LimitToFirst(size_t limit);
  QueryInternal* LimitToLast(size_t limit);

  const internal::QuerySpec& query_spec() const { return query_spec_; }

  static bool Initialize(App* app);
  static void Terminate(App* app);

  DatabaseInternal* database_internal() const { return db_; }

 protected:
  DatabaseInternal* db_;
  jobject obj_;
  internal::QuerySpec query_spec_;

 private:
  ReferenceCountedFutureImpl* query_future();

  // The memory location of this member variable is used to look up our
  // ReferenceCountedFutureImpl. We can't use "this" because QueryInternal and
  // DatabaseReferenceInternal require two separate ReferenceCountedFutureImpl
  // instances, but have the same "this" pointer as one is a subclass of the
  // other.
  int future_api_id_;
};

// Used by Query::GetValue().
class SingleValueListener : public ValueListener {
 public:
  SingleValueListener(DatabaseInternal* db, ReferenceCountedFutureImpl* future,
                      SafeFutureHandle<DataSnapshot> handle);
  // Unregister ourselves from the db.
  virtual ~SingleValueListener();
  virtual void OnValueChanged(const DataSnapshot& snapshot);
  virtual void OnCancelled(const Error& error_code, const char* error_message);
  void SetJavaListener(jobject obj);

 private:
  DatabaseInternal* db_;
  ReferenceCountedFutureImpl* future_;
  SafeFutureHandle<DataSnapshot> handle_;
  jobject java_listener_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_QUERY_ANDROID_H_
