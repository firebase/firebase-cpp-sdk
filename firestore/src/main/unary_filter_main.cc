// Copyright 2023 Google LLC
#include "firestore/src/main/unary_filter_main.h"

#include <utility>
#include <vector>
#include "Firestore/core/src/nanopb/message.h"
#include "firestore/src/main/converter_main.h"

namespace firebase::firestore {

UnaryFilterInternal::UnaryFilterInternal(FieldPath field_path, core::FieldFilter::Operator op,
                                         FieldValue value)
    : FilterInternal(FilterType::Unary),
      allow_arrays_(false),
      path_(std::move(field_path)),
      op_(op),
      value_(std::move(value)) {}

UnaryFilterInternal::UnaryFilterInternal(FieldPath field_path, core::FieldFilter::Operator op,
                                         const std::vector<FieldValue>& values)
    : FilterInternal(FilterType::Unary),
      allow_arrays_(true),
      path_(std::move(field_path)),
      op_(op),
      value_(FieldValue::Array(values)) {}

UnaryFilterInternal* UnaryFilterInternal::clone() const { return new UnaryFilterInternal(*this); }

core::Filter UnaryFilterInternal::ToCoreFilter(
    const api::Query& query, const UserDataConverter& user_data_converter) const {
  FieldValue::Type val_type = value_.type();
  return query.ParseFieldFilter(GetInternal(path_), op_,
                                user_data_converter.ParseQueryValue(value_, allow_arrays_),
                                [val_type] { return Describe(val_type); });
}

bool operator==(const UnaryFilterInternal& lhs, const UnaryFilterInternal& rhs) {
  return lhs.op_ == rhs.op_ && lhs.path_ == rhs.path_ && lhs.value_ == rhs.value_ &&
         lhs.allow_arrays_ == rhs.allow_arrays_;
}

}  // namespace firebase::firestore
