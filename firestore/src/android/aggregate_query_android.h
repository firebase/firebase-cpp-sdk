/*
* Copyright 2022 Google LLC
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

#include "firestore/src/android/wrapper.h"

namespace firebase {
namespace firestore {

class AggregateQueryInternal : public Wrapper {
 public:

  /**
   * @brief Returns the query whose aggregations will be calculated by this
   * object.
   */
  Query query();

  /**
   * @brief Executes the aggregate query and returns the results as a
   * AggregateQuerySnapshot.
   *
   * @param[in] aggregate_source A value to configure the get behavior.
   *
   * @return A Future that will be resolved with the results of the AggregateQuery.
   */
  Future<AggregateQuerySnapshot> Get(AggregateSource aggregate_source);

  size_t Hash() const;

};

bool operator==(const AggregateQueryInternal& lhs, const AggregateQueryInternal& rhs);
inline bool operator!=(const AggregateQueryInternal& lhs, const AggregateQueryInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_AGGREGATE_QUERY_ANDROID_H_
