/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/android/aggregate_query_android.h"

#include "firestore/src/android/aggregate_source_android.h"
#include "firestore/src/jni/compare.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::Task;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/AggregateQuery";
Method<Task> kGet("get",
                  "(Lcom/google/firebase/firestore/AggregateSource;)"
                  "Lcom/google/android/gms/tasks/Task;");
Method<Object> kGetQuery("getQuery", "()Lcom/google/firebase/firestore/Query;");
Method<int32_t> kHashCode("hashCode", "()I");

}  // namespace

void AggregateQueryInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kGet, kGetQuery, kHashCode);
}

Query AggregateQueryInternal::query() const {
  Env env = GetEnv();
  Local<Object> query = env.Call(obj_, kGetQuery);
  return firestore_->NewQuery(env, query);
}

Future<AggregateQuerySnapshot> AggregateQueryInternal::Get(
    AggregateSource aggregate_source) {
  Env env = GetEnv();
  Local<Object> java_source =
      AggregateSourceInternal::Create(env, aggregate_source);
  Local<Task> task = env.Call(obj_, kGet, java_source);
  return promises_.NewFuture<AggregateQuerySnapshot>(env, AsyncFn::kGet, task);
}

std::size_t AggregateQueryInternal::Hash() const {
  Env env = GetEnv();
  return env.Call(obj_, kHashCode);
}

bool operator==(const AggregateQueryInternal& lhs,
                const AggregateQueryInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firestore
}  // namespace firebase
