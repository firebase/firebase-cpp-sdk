// Copyright 2023 Google LLC
#ifndef FIREBASE_FIRESTORE_SRC_MAIN_COMPOSITE_FILTER_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_COMPOSITE_FILTER_MAIN_H_

#if defined(__ANDROID__)
#error "This file should not be used on Android."
#endif

#include <memory>
#include <vector>
#include "Firestore/core/src/api/query_core.h"
#include "firestore/src/main/filter_main.h"

namespace firebase::firestore {

class CompositeFilterInternal : public FilterInternal {
 public:
  CompositeFilterInternal(core::CompositeFilter::Operator op,
                          std::vector<std::unique_ptr<FilterInternal>> filters);

  core::Filter ToCoreFilter(const api::Query& query,
                            const UserDataConverter& user_data_converter) const override;
  friend bool operator==(const CompositeFilterInternal& lhs, const CompositeFilterInternal& rhs);

 protected:
  bool IsEmpty() const override;

 private:
  CompositeFilterInternal* clone() const override;

  const core::CompositeFilter::Operator op_;
  std::vector<std::shared_ptr<FilterInternal>> filters_;
};

bool operator==(const CompositeFilterInternal& lhs, const CompositeFilterInternal& rhs);
inline bool operator!=(const CompositeFilterInternal& lhs, const CompositeFilterInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_COMPOSITE_FILTER_MAIN_H_
