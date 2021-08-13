/*
 * Copyright 2018 Google LLC
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

#include "firestore/src/include/firebase/firestore/field_path.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "app/meta/move.h"

#if defined(__ANDROID__)
#include "firestore/src/android/field_path_portable.h"
#else
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/util/hashing.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

FieldPath::FieldPath() {}

FieldPath::FieldPath(std::initializer_list<std::string> field_names)
    : internal_(InternalFromSegments(std::vector<std::string>(field_names))) {}

FieldPath::FieldPath(const std::vector<std::string>& field_names)
    : internal_(InternalFromSegments(field_names)) {}

FieldPath::FieldPath(const FieldPath& path)
    : internal_(new FieldPathInternal{*path.internal_}) {}

FieldPath::FieldPath(FieldPath&& path) noexcept : internal_(path.internal_) {
  path.internal_ = nullptr;
}

FieldPath::FieldPath(FieldPathInternal* internal) : internal_(internal) {}

FieldPath::~FieldPath() {
  delete internal_;
  internal_ = nullptr;
}

FieldPath& FieldPath::operator=(const FieldPath& path) {
  if (this == &path) {
    return *this;
  }

  delete internal_;
  internal_ = new FieldPathInternal{*path.internal_};
  return *this;
}

FieldPath& FieldPath::operator=(FieldPath&& path) noexcept {
  std::swap(internal_, path.internal_);
  return *this;
}

/* static */
FieldPath FieldPath::DocumentId() {
  return FieldPath{new FieldPathInternal{FieldPathInternal::KeyFieldPath()}};
}

FieldPath::FieldPathInternal* FieldPath::InternalFromSegments(
    std::vector<std::string> field_names) {
  return new FieldPathInternal(
      FieldPathInternal::FromSegments(Move(field_names)));
}

/* static */
FieldPath FieldPath::FromDotSeparatedString(const std::string& path) {
  return FieldPath{
      new FieldPathInternal{FieldPathInternal::FromDotSeparatedString(path)}};
}

std::string FieldPath::ToString() const {
  if (!internal_) return "";
  return internal_->CanonicalString();
}

std::ostream& operator<<(std::ostream& out, const FieldPath& path) {
  return out << path.ToString();
}

bool operator==(const FieldPath& lhs, const FieldPath& rhs) {
  if (!lhs.internal_ || !rhs.internal_) {
    return lhs.internal_ == rhs.internal_;
  }

  return *lhs.internal_ == *rhs.internal_;
}

bool operator!=(const FieldPath& lhs, const FieldPath& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

namespace std {
#if defined(__ANDROID__)
size_t hash<firebase::firestore::FieldPath>::operator()(
    const firebase::firestore::FieldPath& field_path) const {
  size_t hash = 1;
  for (const auto& segment : *field_path.internal_) {
    hash = 31 * hash + std::hash<std::string>{}(segment);
  }
  return hash;
}
#else
size_t hash<firebase::firestore::FieldPath>::operator()(
    const firebase::firestore::FieldPath& field_path) const {
  return firebase::firestore::util::Hash(*field_path.internal_);
}
#endif  // defined(__ANDROID__)
}  // namespace std
