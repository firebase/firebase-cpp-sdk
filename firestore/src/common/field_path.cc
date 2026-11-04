// Copyright 2018 Google LLC
#include "firestore/src/include/firebase/firestore/field_path.h"

#include <algorithm>
#include <ostream>
#include <utility>
#if defined(__ANDROID__)
#include "firestore/src/android/field_path_portable.h"
#else
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/util/hashing.h"
#endif

namespace firebase::firestore {

FieldPath::FieldPath() {}
FieldPath::FieldPath(std::initializer_list<std::string> field_names)
    : internal_(InternalFromSegments(std::vector<std::string>(field_names))) {}
FieldPath::FieldPath(const std::vector<std::string>& field_names)
    : internal_(InternalFromSegments(field_names)) {}
FieldPath::FieldPath(const FieldPath& path)
    : internal_(path.internal_ ? new FieldPathInternal{*path.internal_} : nullptr) {}
FieldPath::FieldPath(FieldPath&& path) noexcept
    : internal_(std::exchange(path.internal_, nullptr)) {}
FieldPath::FieldPath(FieldPathInternal* internal) : internal_(internal) {}
FieldPath::~FieldPath() { delete std::exchange(internal_, nullptr); }

FieldPath& FieldPath::operator=(const FieldPath& path) {
  if (this == &path) return *this;
  delete std::exchange(internal_, path.internal_ ? new FieldPathInternal{*path.internal_} : nullptr);
  return *this;
}

FieldPath& FieldPath::operator=(FieldPath&& path) noexcept {
  if (this == &path) return *this;
  delete std::exchange(internal_, std::exchange(path.internal_, nullptr));
  return *this;
}

FieldPath FieldPath::DocumentId() {
  return FieldPath{new FieldPathInternal{FieldPathInternal::KeyFieldPath()}};
}

FieldPath::FieldPathInternal* FieldPath::InternalFromSegments(
    std::vector<std::string> field_names) {
  return new FieldPathInternal(FieldPathInternal::FromSegments(std::move(field_names)));
}

FieldPath FieldPath::FromDotSeparatedString(const std::string& path) {
  return FieldPath{new FieldPathInternal{FieldPathInternal::FromDotSeparatedString(path)}};
}

std::string FieldPath::ToString() const {
  return internal_ ? internal_->CanonicalString() : "";
}

std::ostream& operator<<(std::ostream& out, const FieldPath& path) {
  return out << path.ToString();
}

bool operator==(const FieldPath& lhs, const FieldPath& rhs) {
  if (!lhs.internal_ || !rhs.internal_) return lhs.internal_ == rhs.internal_;
  return *lhs.internal_ == *rhs.internal_;
}

bool operator!=(const FieldPath& lhs, const FieldPath& rhs) { return !(lhs == rhs); }

}  // namespace firebase::firestore

namespace std {
size_t hash<firebase::firestore::FieldPath>::operator()(
    const firebase::firestore::FieldPath& field_path) const {
  if (!field_path.internal_) return 0;
#if defined(__ANDROID__)
  size_t hash = 1;
  for (const auto& segment : *field_path.internal_) {
    hash = 31 * hash + std::hash<std::string>{}(segment);
  }
  return hash;
#else
  return firebase::firestore::util::Hash(*field_path.internal_);
#endif
}
}  // namespace std
