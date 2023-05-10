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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_AGGREGATE_QUERY_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_AGGREGATE_QUERY_ANDROID_H_

#include "firestore/src/android/promise_factory_android.h"
#include "firestore/src/android/query_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/aggregate_source.h"

namespace firebase {
namespace firestore {

class AggregateQueryInternal : public Wrapper {
 public:
  // Each API of AggregateQuery that returns a Future needs to define an enum
  // value here. For example, a Future-returning method Foo() relies on the enum
  // value kFoo. The enum values are used to identify and manage Future in the
  // Firestore Future manager.
  enum class AsyncFn {
    kGet = 0,
    kCount,  // Must be the last enum value.
  };

  static void Initialize(jni::Loader& loader);

  AggregateQueryInternal(FirestoreInternal* firestore,
                         const jni::Object& object)
      : Wrapper(firestore, object), promises_(firestore) {}

  /**
   * @brief Returns the query whose aggregations will be calculated by this
   * object.
   */
  Query query() const;

  /**
   * @brief Executes the aggregate query and returns the results as a
   * AggregateQuerySnapshot.
   *
   * @param[in] aggregate_source A value to configure the get behavior.
   *
   * @return A Future that will be resolved with the results of the
   * AggregateQuery.
   */
  Future<AggregateQuerySnapshot> Get(AggregateSource aggregate_source);

  size_t Hash() const;

 private:
  PromiseFactory<AsyncFn> promises_;
};

bool operator==(const AggregateQueryInternal& lhs,
                const AggregateQueryInternal& rhs);
inline bool operator!=(const AggregateQueryInternal& lhs,
                       const AggregateQueryInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_AGGREGATE_QUERY_ANDROID_H_
