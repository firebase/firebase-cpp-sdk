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

#ifndef FIREBASE_DATABASE_SRC_ANDROID_QUERY_ANDROID_H_
#define FIREBASE_DATABASE_SRC_ANDROID_QUERY_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/common/query.h"
#include "database/src/common/query_spec.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/listener.h"

namespace firebase {
namespace database {
namespace internal {

class QueryInternal {
 public:
  QueryInternal(DatabaseInternal* database, jobject query_obj);
  QueryInternal(DatabaseInternal* database, jobject query_obj,
                const internal::QuerySpec& query_spec);
  QueryInternal(const QueryInternal& query);
  QueryInternal& operator=(const QueryInternal& query);
  QueryInternal(QueryInternal&& query);
  QueryInternal& operator=(QueryInternal&& query);
  virtual ~QueryInternal();

  void GetValueImpl(SafeFutureHandle<DataSnapshot> handle);
  void AddValueListener(ValueListener* listener);
  void RemoveValueListener(ValueListener* listener);
  void RemoveAllValueListeners();
  void AddChildListener(ChildListener* listener);
  void RemoveChildListener(ChildListener* listener);
  void RemoveAllChildListeners();

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
  ReferenceCountedFutureImpl* query_future() { return future_api_.get(); }

 protected:
  DatabaseInternal* db_;
  jobject obj_;
  internal::QuerySpec query_spec_;

 private:
  QueryInternal* CallRangeMethod(const Variant& value, const char* key,
                                 const internal::QuerySpec& spec,
                                 int bool_method, int double_method,
                                 int str_method, const char* fn_name);
  ScopedFutureApi<DatabaseInternal, kQueryFnCount> future_api_;
};

class SingleValueListener : public ValueListener {
 public:
  SingleValueListener(DatabaseInternal* db, ReferenceCountedFutureImpl* future,
                      SafeFutureHandle<DataSnapshot> handle);
  ~SingleValueListener() override;
  void OnValueChanged(const DataSnapshot& snapshot) override;
  void OnCancelled(const Error& error_code, const char* error_message) override;
  void SetJavaListener(jobject obj);

 private:
  void CleanupJavaListener();
  DatabaseInternal* db_;
  ReferenceCountedFutureImpl* future_;
  SafeFutureHandle<DataSnapshot> handle_;
  jobject java_listener_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_ANDROID_QUERY_ANDROID_H_
