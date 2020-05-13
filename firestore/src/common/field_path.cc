/*
 * Copyright 2018 Google
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
#if defined(_STLPORT_VERSION)
// STLPort defines `std::hash` in the `unordered_` headers.
#include <unordered_set>
#endif  // defined(_STLPORT_VERSION)

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/android/field_path_portable.h"
#else
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/util/hashing.h"
#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

namespace firebase {
namespace firestore {

FieldPath::FieldPath() {}

#if !defined(_STLPORT_VERSION)
FieldPath::FieldPath(std::initializer_list<std::string> field_names)
    : internal_(new FieldPathInternal{field_names}) {}
#endif  // !defined(_STLPORT_VERSION)

FieldPath::FieldPath(const std::vector<std::string>& field_names)
    : internal_(new FieldPathInternal{std::vector<std::string>{field_names}}) {}

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
#if defined(_STLPORT_VERSION)
size_t hash<firebase::firestore::FieldPath>::operator()(
    const firebase::firestore::FieldPath& field_path) const {
  size_t hash = 1;
  for (const auto& segment : *field_path.internal_) {
    // STLPort doesn't define `std::hash<std::string>`, but the `const char*`
    // specialization handles C-style strings, not just the pointer value.
    hash = 31 * hash + std::hash<const char*>{}(segment.c_str());
  }
  return hash;
}

#elif defined(__ANDROID__)
size_t hash<firebase::firestore::FieldPath>::operator()(
    const firebase::firestore::FieldPath& field_path) const {
  size_t hash = 1;
  for (const auto& segment : *field_path.internal_) {
    hash = 31 * hash + std::hash<std::string>{}(segment);
  }
  return hash;
}
#elif !defined(FIRESTORE_STUB_BUILD)
size_t hash<firebase::firestore::FieldPath>::operator()(
    const firebase::firestore::FieldPath& field_path) const {
  return firebase::firestore::util::Hash(*field_path.internal_);
}

#else
// Stub
size_t hash<firebase::firestore::FieldPath>::operator()(
    const firebase::firestore::FieldPath&) const {
  return 0;
}
#endif  // defined(_STLPORT_VERSION)
}  // namespace std
