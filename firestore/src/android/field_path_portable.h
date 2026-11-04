// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_PORTABLE_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_PORTABLE_H_

#include <string>
#include <utility>
#include <vector>

#include "app/src/assert.h"

namespace firebase::firestore {

class FieldPathPortable {
 public:
  using const_iterator = std::vector<std::string>::const_iterator;
  static constexpr const char* kDocumentKeyPath = "__name__";

  explicit FieldPathPortable(std::vector<std::string> segments)
      : segments_(std::move(segments)) {}

  [[nodiscard]] const std::string& operator[](const size_t i) const {
    FIREBASE_ASSERT_MESSAGE(i < segments_.size(), "index %zu out of range", i);
    return segments_[i];
  }
  [[nodiscard]] const_iterator begin() const { return segments_.begin(); }
  [[nodiscard]] const_iterator end() const { return segments_.end(); }
  [[nodiscard]] size_t size() const { return segments_.size(); }
  [[nodiscard]] std::string CanonicalString() const;
  [[nodiscard]] bool IsKeyFieldPath() const;

  bool operator==(const FieldPathPortable& rhs) const {
    return segments_ == rhs.segments_;
  }
  bool operator!=(const FieldPathPortable& rhs) const {
    return segments_ != rhs.segments_;
  }
  bool operator<(const FieldPathPortable& rhs) const {
    return segments_ < rhs.segments_;
  }
  bool operator>(const FieldPathPortable& rhs) const {
    return segments_ > rhs.segments_;
  }
  bool operator<=(const FieldPathPortable& rhs) const {
    return segments_ <= rhs.segments_;
  }
  bool operator>=(const FieldPathPortable& rhs) const {
    return segments_ >= rhs.segments_;
  }

  [[nodiscard]] static FieldPathPortable FromSegments(
      std::vector<std::string> segments);
  [[nodiscard]] static FieldPathPortable FromDotSeparatedString(
      const std::string& path);
  [[nodiscard]] static FieldPathPortable KeyFieldPath();

 private:
  std::vector<std::string> segments_;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_PORTABLE_H_
