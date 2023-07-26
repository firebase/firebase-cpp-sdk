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

#include "firebase/firestore/filter.h"

#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/hard_assert_common.h"

#if defined(__ANDROID__)
#include "firestore/src/android/filter_android.h"
#else
#include "firestore/src/main/filter_main.h"
#endif  // defined(__ANDROID__)

#include "firestore/src/common/util.h"

namespace firebase {
namespace firestore {

Filter::Filter(const Filter& other) { internal_ = other.internal_->clone(); }

Filter::Filter(Filter&& other) noexcept {
  std::swap(internal_, other.internal_);
}

Filter::Filter(FilterInternal* internal) : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
}

Filter::~Filter() {
  delete internal_;
  internal_ = nullptr;
}

Filter& Filter::operator=(const Filter& other) {
  if (this == &other) {
    return *this;
  }
  delete internal_;
  internal_ = other.internal_->clone();
  return *this;
}

Filter& Filter::operator=(Filter&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  delete internal_;
  internal_ = other.internal_;
  other.internal_ = nullptr;
  return *this;
}

Filter Filter::ArrayContains(const std::string& field,
                             const FieldValue& value) {
  return ArrayContains(FieldPath::FromDotSeparatedString(field), value);
}

Filter Filter::ArrayContainsAny(const std::string& field,
                                const std::vector<FieldValue>& values) {
  return ArrayContainsAny(FieldPath::FromDotSeparatedString(field), values);
}

Filter Filter::EqualTo(const std::string& field,
                       const firebase::firestore::FieldValue& value) {
  return EqualTo(FieldPath::FromDotSeparatedString(field), value);
}

Filter Filter::NotEqualTo(const std::string& field, const FieldValue& value) {
  return NotEqualTo(FieldPath::FromDotSeparatedString(field), value);
}

Filter Filter::GreaterThan(const std::string& field, const FieldValue& value) {
  return GreaterThan(FieldPath::FromDotSeparatedString(field), value);
}

Filter Filter::GreaterThanOrEqualTo(const std::string& field,
                                    const FieldValue& value) {
  return GreaterThanOrEqualTo(FieldPath::FromDotSeparatedString(field), value);
}

Filter Filter::LessThan(const std::string& field, const FieldValue& value) {
  return LessThan(FieldPath::FromDotSeparatedString(field), value);
}

Filter Filter::LessThanOrEqualTo(const std::string& field,
                                 const FieldValue& value) {
  return LessThanOrEqualTo(FieldPath::FromDotSeparatedString(field), value);
}

Filter Filter::In(const std::string& field,
                  const std::vector<FieldValue>& values) {
  return In(FieldPath::FromDotSeparatedString(field), values);
}

Filter Filter::NotIn(const std::string& field,
                     const std::vector<FieldValue>& values) {
  return NotIn(FieldPath::FromDotSeparatedString(field), values);
}

Filter Filter::ArrayContains(const FieldPath& field, const FieldValue& value) {
  return FilterInternal::ArrayContains(field, value);
}

Filter Filter::ArrayContainsAny(const FieldPath& field,
                                const std::vector<FieldValue>& values) {
  return FilterInternal::ArrayContainsAny(field, values);
}

Filter Filter::EqualTo(const FieldPath& field, const FieldValue& value) {
  return FilterInternal::EqualTo(field, value);
}

Filter Filter::NotEqualTo(const FieldPath& field, const FieldValue& value) {
  return FilterInternal::NotEqualTo(field, value);
}

Filter Filter::GreaterThan(const FieldPath& field, const FieldValue& value) {
  return FilterInternal::GreaterThan(field, value);
}

Filter Filter::GreaterThanOrEqualTo(const FieldPath& field,
                                    const FieldValue& value) {
  return FilterInternal::GreaterThanOrEqualTo(field, value);
}

Filter Filter::LessThan(const FieldPath& field, const FieldValue& value) {
  return FilterInternal::LessThan(field, value);
}

Filter Filter::LessThanOrEqualTo(const FieldPath& field,
                                 const FieldValue& value) {
  return FilterInternal::LessThanOrEqualTo(field, value);
}

Filter Filter::In(const FieldPath& field,
                  const std::vector<FieldValue>& values) {
  return FilterInternal::In(field, values);
}

Filter Filter::NotIn(const FieldPath& field,
                     const std::vector<FieldValue>& values) {
  return FilterInternal::NotIn(field, values);
}

Filter Filter::And(const std::vector<Filter>& filters) {
  return FilterInternal::And(filters);
}

Filter Filter::Or(const std::vector<Filter>& filters) {
  return FilterInternal::Or(filters);
}

bool operator==(const Filter& lhs, const Filter& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

bool Filter::IsEmpty() const { return internal_->IsEmpty(); }

}  // namespace firestore
}  // namespace firebase
