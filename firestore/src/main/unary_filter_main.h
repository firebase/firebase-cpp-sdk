// Copyright 2023 Google LLC
#ifndef FIREBASE_FIRESTORE_SRC_MAIN_UNARY_FILTER_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_UNARY_FILTER_MAIN_H_

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

#include <vector>
#include "Firestore/core/src/api/query_core.h"
#include "firestore/src/main/filter_main.h"

namespace firebase::firestore {

class UnaryFilterInternal final : public FilterInternal {
 public:
  UnaryFilterInternal(FieldPath field_path, core::FieldFilter::Operator op, FieldValue value);
  UnaryFilterInternal(FieldPath field_path, core::FieldFilter::Operator op,
                      const std::vector<FieldValue>& values);

  core::Filter ToCoreFilter(const api::Query& query,
                            const UserDataConverter& user_data_converter) const override;
  friend bool operator==(const UnaryFilterInternal& lhs, const UnaryFilterInternal& rhs);

 protected:
  bool IsEmpty() const override { return false; }

 private:
  UnaryFilterInternal* clone() const override;

  const bool allow_arrays_ = false;
  const FieldPath path_;
  const core::FieldFilter::Operator op_;
  const FieldValue value_;
};

bool operator==(const UnaryFilterInternal& lhs, const UnaryFilterInternal& rhs);
inline bool operator!=(const UnaryFilterInternal& lhs, const UnaryFilterInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_UNARY_FILTER_MAIN_H_
