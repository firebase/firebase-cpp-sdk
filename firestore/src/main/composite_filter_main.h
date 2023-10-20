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

#include <memory>
#include <vector>

#include "Firestore/core/src/api/query_core.h"
#include "firestore/src/main/filter_main.h"

namespace firebase {
namespace firestore {

class CompositeFilterInternal : public FilterInternal {
 public:
  CompositeFilterInternal(core::CompositeFilter::Operator op,
                          std::vector<FilterInternal*>& filters);

  core::Filter ToCoreFilter(const api::Query& query,
                            const firebase::firestore::UserDataConverter&
                                user_data_converter) const override;

  friend bool operator==(const CompositeFilterInternal& lhs,
                         const CompositeFilterInternal& rhs);

 protected:
  bool IsEmpty() const override;

 private:
  CompositeFilterInternal* clone() override;

  const core::CompositeFilter::Operator op_;
  std::vector<std::shared_ptr<FilterInternal>> filters_;
};

bool operator==(const CompositeFilterInternal& lhs,
                const CompositeFilterInternal& rhs);

inline bool operator!=(const CompositeFilterInternal& lhs,
                       const CompositeFilterInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_COMPOSITE_FILTER_MAIN_H_
