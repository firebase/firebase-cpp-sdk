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

#include <utility>
#include <vector>

#include "firestore/src/main/unary_filter_main.h"

#include "Firestore/core/src/nanopb/message.h"
#include "firestore/src/main/converter_main.h"

namespace firebase {
namespace firestore {

using nanopb::Message;

UnaryFilterInternal::UnaryFilterInternal(FieldPath field_path,
                                         core::FieldFilter::Operator op,
                                         FieldValue value)
    : FilterInternal(FilterType::Unary),
      allow_arrays_(false),
      path_(std::move(field_path)),
      op_(op),
      value_(std::move(value)) {}

UnaryFilterInternal::UnaryFilterInternal(FieldPath field_path,
                                         core::FieldFilter::Operator op,
                                         const std::vector<FieldValue>& values)
    : FilterInternal(FilterType::Unary),
      allow_arrays_(true),
      path_(std::move(field_path)),
      op_(op),
      value_(FieldValue::Array(values)) {}

UnaryFilterInternal* UnaryFilterInternal::clone() {
  return new UnaryFilterInternal(*this);
}

core::Filter UnaryFilterInternal::ToCoreFilter(
    const api::Query& query,
    const firebase::firestore::UserDataConverter& user_data_converter) const {
  const model::FieldPath& path = GetInternal(path_);
  Message<google_firestore_v1_Value> parsed =
      user_data_converter.ParseQueryValue(value_, allow_arrays_);
  auto describer = [this] { return Describe(value_.type()); };

  return query.ParseFieldFilter(path, op_, std::move(parsed), describer);
}

bool operator==(const UnaryFilterInternal& lhs,
                const UnaryFilterInternal& rhs) {
  return lhs.op_ == rhs.op_ && lhs.path_ == rhs.path_ &&
         lhs.value_ == rhs.value_;
}

}  // namespace firestore
}  // namespace firebase
