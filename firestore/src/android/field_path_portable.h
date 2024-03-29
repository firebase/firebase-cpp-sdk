/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_PORTABLE_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_PORTABLE_H_

#include <string>
#include <vector>

#include <utility>
#include "app/src/assert.h"

namespace firebase {
namespace firestore {

/**
 * A dot-separated path for navigating sub-objects within a document. This
 * implementation is for the option STLPort, which has limited support of the
 * standard library and thus cannot depends on Firestore core C++ library.
 */
class FieldPathPortable {
 public:
  using const_iterator = std::vector<std::string>::const_iterator;

  /** The field path string that represents the document's key. */
  static constexpr const char* kDocumentKeyPath = "__name__";

  explicit FieldPathPortable(std::vector<std::string>&& segments)
      : segments_(std::move(segments)) {}

  /** Returns i-th segment of the path. */
  const std::string& operator[](const size_t i) const {
    FIREBASE_ASSERT_MESSAGE(i < segments_.size(), "index %s out of range", i);
    return segments_[i];
  }

  const_iterator begin() const { return segments_.begin(); }

  const_iterator end() const { return segments_.end(); }

  size_t size() const { return segments_.size(); }

  std::string CanonicalString() const;

  bool IsKeyFieldPath() const;

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

  /**
   * Creates and returns a new path from an explicitly pre-split list of
   * segments.
   */
  static FieldPathPortable FromSegments(std::vector<std::string> segments);

  /**
   * Creates and returns a new path from a dot-separated field-path string,
   * where path segments are separated by a dot ".".
   */
  static FieldPathPortable FromDotSeparatedString(const std::string& path);

  static FieldPathPortable KeyFieldPath();

 private:
  std::vector<std::string> segments_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_PORTABLE_H_
