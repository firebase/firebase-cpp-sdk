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

#include "firestore/src/android/aggregate_query_snapshot_android.h"

#include "firestore/src/android/aggregate_query_android.h"
#include "firestore/src/jni/compare.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticMethod;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/AggregateQuerySnapshot";
Constructor<Object> kConstructor(
    "(Lcom/google/firebase/firestore/AggregateQuery;Ljava/util/Map;)V");
Method<int64_t> kGetCount("getCount", "()J");
Method<Object> kGetQuery("getQuery",
                         "()Lcom/google/firebase/firestore/AggregateQuery;");
Method<int32_t> kHashCode("hashCode", "()I");

constexpr char kHelperClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/AggregateQuerySnapshotHelper";
StaticMethod<Object> kCreateConstructorArg(
    "createAggregateQuerySnapshotConstructorArg", "(J)Ljava/util/Map;");

}  // namespace

void AggregateQuerySnapshotInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kConstructor, kGetCount, kGetQuery, kHashCode);
  loader.LoadClass(kHelperClassName, kCreateConstructorArg);
}

AggregateQuerySnapshot AggregateQuerySnapshotInternal::Create(
    Env& env, AggregateQueryInternal& aggregate_query_internal, int64_t count) {
  Local<Object> snapshot_data = env.Call(kCreateConstructorArg, count);
  Local<Object> instance =
      env.New(kConstructor, aggregate_query_internal.ToJava(), snapshot_data);
  return aggregate_query_internal.firestore_internal()
      ->NewAggregateQuerySnapshot(env, instance);
}

AggregateQuery AggregateQuerySnapshotInternal::query() const {
  Env env = GetEnv();
  Local<Object> query = env.Call(obj_, kGetQuery);
  return firestore_->NewAggregateQuery(env, query);
}

int64_t AggregateQuerySnapshotInternal::count() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetCount);
}

std::size_t AggregateQuerySnapshotInternal::Hash() const {
  Env env = GetEnv();
  return env.Call(obj_, kHashCode);
}

bool operator==(const AggregateQuerySnapshotInternal& lhs,
                const AggregateQuerySnapshotInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firestore
}  // namespace firebase
