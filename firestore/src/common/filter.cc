// Copyright 2023 Google LLC
#include "firebase/firestore/filter.h"

#include <utility>
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/util.h"
#if defined(__ANDROID__)
#include "firestore/src/android/filter_android.h"
#else
#include "firestore/src/main/filter_main.h"
#endif

namespace firebase::firestore {
namespace {

void ValidateFieldAndValue(const FieldPath& field, const FieldValue& value) {
  if (!field.is_valid()) SimpleThrowInvalidArgument("Field path cannot be invalid.");
  if (!value.is_valid()) SimpleThrowInvalidArgument("Field value cannot be invalid.");
}

void ValidateFieldAndValues(const FieldPath& field, const std::vector<FieldValue>& values) {
  if (!field.is_valid()) SimpleThrowInvalidArgument("Field path cannot be invalid.");
  for (const auto& value : values) {
    if (!value.is_valid()) SimpleThrowInvalidArgument("Field value cannot be invalid.");
  }
}

}  // namespace

Filter::Filter(const Filter& other)
    : internal_(other.internal_ ? other.internal_->clone() : nullptr) {}

Filter::Filter(Filter&& other) noexcept { std::swap(internal_, other.internal_); }

Filter::Filter(FilterInternal* internal) : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
}

Filter::~Filter() { delete std::exchange(internal_, nullptr); }

Filter& Filter::operator=(const Filter& other) {
  if (this == &other) return *this;
  delete std::exchange(internal_, other.internal_ ? other.internal_->clone() : nullptr);
  return *this;
}

Filter& Filter::operator=(Filter&& other) noexcept {
  if (this == &other) return *this;
  delete std::exchange(internal_, std::exchange(other.internal_, nullptr));
  return *this;
}

Filter Filter::ArrayContains(const std::string& field, const FieldValue& value) {
  return ArrayContains(FieldPath::FromDotSeparatedString(field), value);
}
Filter Filter::ArrayContainsAny(const std::string& field, const std::vector<FieldValue>& values) {
  return ArrayContainsAny(FieldPath::FromDotSeparatedString(field), values);
}
Filter Filter::EqualTo(const std::string& field, const FieldValue& value) {
  return EqualTo(FieldPath::FromDotSeparatedString(field), value);
}
Filter Filter::NotEqualTo(const std::string& field, const FieldValue& value) {
  return NotEqualTo(FieldPath::FromDotSeparatedString(field), value);
}
Filter Filter::GreaterThan(const std::string& field, const FieldValue& value) {
  return GreaterThan(FieldPath::FromDotSeparatedString(field), value);
}
Filter Filter::GreaterThanOrEqualTo(const std::string& field, const FieldValue& value) {
  return GreaterThanOrEqualTo(FieldPath::FromDotSeparatedString(field), value);
}
Filter Filter::LessThan(const std::string& field, const FieldValue& value) {
  return LessThan(FieldPath::FromDotSeparatedString(field), value);
}
Filter Filter::LessThanOrEqualTo(const std::string& field, const FieldValue& value) {
  return LessThanOrEqualTo(FieldPath::FromDotSeparatedString(field), value);
}
Filter Filter::In(const std::string& field, const std::vector<FieldValue>& values) {
  return In(FieldPath::FromDotSeparatedString(field), values);
}
Filter Filter::NotIn(const std::string& field, const std::vector<FieldValue>& values) {
  return NotIn(FieldPath::FromDotSeparatedString(field), values);
}

Filter Filter::ArrayContains(const FieldPath& field, const FieldValue& value) {
  ValidateFieldAndValue(field, value);
  return FilterInternal::ArrayContains(field, value);
}
Filter Filter::ArrayContainsAny(const FieldPath& field, const std::vector<FieldValue>& values) {
  ValidateFieldAndValues(field, values);
  return FilterInternal::ArrayContainsAny(field, values);
}
Filter Filter::EqualTo(const FieldPath& field, const FieldValue& value) {
  ValidateFieldAndValue(field, value);
  return FilterInternal::EqualTo(field, value);
}
Filter Filter::NotEqualTo(const FieldPath& field, const FieldValue& value) {
  ValidateFieldAndValue(field, value);
  return FilterInternal::NotEqualTo(field, value);
}
Filter Filter::GreaterThan(const FieldPath& field, const FieldValue& value) {
  ValidateFieldAndValue(field, value);
  return FilterInternal::GreaterThan(field, value);
}
Filter Filter::GreaterThanOrEqualTo(const FieldPath& field, const FieldValue& value) {
  ValidateFieldAndValue(field, value);
  return FilterInternal::GreaterThanOrEqualTo(field, value);
}
Filter Filter::LessThan(const FieldPath& field, const FieldValue& value) {
  ValidateFieldAndValue(field, value);
  return FilterInternal::LessThan(field, value);
}
Filter Filter::LessThanOrEqualTo(const FieldPath& field, const FieldValue& value) {
  ValidateFieldAndValue(field, value);
  return FilterInternal::LessThanOrEqualTo(field, value);
}
Filter Filter::In(const FieldPath& field, const std::vector<FieldValue>& values) {
  ValidateFieldAndValues(field, values);
  return FilterInternal::In(field, values);
}
Filter Filter::NotIn(const FieldPath& field, const std::vector<FieldValue>& values) {
  ValidateFieldAndValues(field, values);
  return FilterInternal::NotIn(field, values);
}
Filter Filter::And(const std::vector<Filter>& filters) { return FilterInternal::And(filters); }
Filter Filter::Or(const std::vector<Filter>& filters) { return FilterInternal::Or(filters); }

bool operator==(const Filter& lhs, const Filter& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

bool Filter::IsEmpty() const { return !internal_ || internal_->IsEmpty(); }

}  // namespace firebase::firestore
