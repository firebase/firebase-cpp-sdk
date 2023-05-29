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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_COMPOSITE_FILTER_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_COMPOSITE_FILTER_MAIN_H_

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

#include "Firestore/core/src/api/query_core.h"
#include "firestore/src/main/filter_main.h"

namespace firebase {
namespace firestore {

class CompositeFilterInternal : public FilterInternal {
 public:
  template <typename... Ts>
  CompositeFilterInternal(core::CompositeFilter::Operator op,
                          const Filter& filter,
                          Ts... filters);

  core::Filter filter_core(
      const api::Query& query,
      const UserDataConverter& user_data_converter) const override;

  friend bool operator==(const CompositeFilterInternal& lhs,
                         const CompositeFilterInternal& rhs);

 private:
  CompositeFilterInternal* clone() override;

  const core::CompositeFilter::Operator op_;
  const std::vector<std::shared_ptr<FilterInternal>> filters_;
};

inline bool operator!=(const CompositeFilterInternal& lhs,
                       const CompositeFilterInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_COMPOSITE_FILTER_MAIN_H_
