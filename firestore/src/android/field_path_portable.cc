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

#include "firestore/src/android/field_path_portable.h"

#include <cctype>
#include <cstring>
#include <sstream>

#include "firestore/src/common/exception_common.h"

namespace firebase {
namespace firestore {
namespace {

/**
 * True if the string could be used as a segment in a field path without
 * escaping. Valid identifies follow the regex [a-zA-Z_][a-zA-Z0-9_]*
 */
bool IsValidFieldPathSegment(const std::string& segment) {
  if (segment.empty()) {
    return false;
  }

  auto iter = segment.begin();
  unsigned char first = static_cast<unsigned char>(*iter);
  if (first != '_' && !std::isalpha(first)) {
    return false;
  }
  ++iter;
  while (iter != segment.end()) {
    unsigned char ch = static_cast<unsigned char>(*iter);
    if (ch != '_' && !std::isalnum(ch)) {
      return false;
    }
    ++iter;
  }
  return true;
}

/**
 * Escape the segment. The escaping logic comes from Firestore C++ core function
 * JoinEscaped().
 */
std::string Escape(const std::string& segment) {
  if (IsValidFieldPathSegment(segment)) {
    return segment;
  }

  std::string result;
  result.reserve(segment.size() * 2 + 2);
  result.push_back('`');
  for (auto iter = segment.begin(); iter != segment.end(); ++iter) {
    if (*iter == '\\' || *iter == '`') {
      result.push_back('\\');
    }
    result.push_back(*iter);
  }
  result.push_back('`');
  return result;
}

// Returns a vector of strings where each string corresponds to a dot-separated
// segment of the given `input`. Any empty segment in `input` will fail
// validation, resulting in an assertion firing. Having no dots in `input` is
// valid.
std::vector<std::string> SplitOnDots(const std::string& input) {
  auto fail_validation = [&input] {
    std::string message = "Invalid field path (" + input +
                          "). Paths must not be empty, begin with '.', end "
                          "with '.', or contain '..'";
    SimpleThrowInvalidArgument(message);
  };

  // `std::getline()` considers empty input to contain zero segments, and if
  // input ends with the separator followed by nothing, that is also not
  // considered a segment. Consequently, these cases are not covered by
  // validation in the body of the loop below and have to be checked beforehand.
  if (input.empty() || input[0] == '.' || input[input.size() - 1] == '.') {
    fail_validation();
    return {};
  }

  std::vector<std::string> result;
  std::string current_segment;
  std::istringstream stream(input);

  while (std::getline(stream, current_segment, '.')) {
    if (current_segment.empty()) {
      fail_validation();
      return {};
    }

    result.push_back(std::move(current_segment));
  }

  return result;
}

bool ValidateSegments(const std::vector<std::string>& segments) {
  if (segments.empty()) {
    SimpleThrowInvalidArgument(
        "Invalid field path. Provided names must not be empty.");
    return false;
  }

  for (size_t i = 0; i < segments.size(); ++i) {
    if (segments[i].empty()) {
      std::ostringstream message;
      message << "Invalid field name at index " << i
              << ". Field names must not be empty.";
      SimpleThrowInvalidArgument(message.str());
      return false;
    }
  }
  return true;
}

}  // anonymous namespace

std::string FieldPathPortable::CanonicalString() const {
  std::string result;
  for (size_t i = 0; i < segments_.size(); ++i) {
    if (i > 0) {
      result.push_back('.');
    }
    result.append(Escape(segments_[i]));
  }
  return result;
}

bool FieldPathPortable::IsKeyFieldPath() const {
  return size() == 1 && segments_[0] == FieldPathPortable::kDocumentKeyPath;
}

FieldPathPortable FieldPathPortable::FromSegments(
    std::vector<std::string> segments) {
  if (!ValidateSegments(segments)) {
    return FieldPathPortable(std::vector<std::string>{});
  }
  return FieldPathPortable(std::move(segments));
}

FieldPathPortable FieldPathPortable::FromDotSeparatedString(
    const std::string& path) {
  if (path.find_first_of("~*/[]") != std::string::npos) {
    std::string message =
        "Invalid field path (" + path +
        "). Paths must not contain '~', '*', '/', '[', or ']'";
    SimpleThrowInvalidArgument(message);
    return FieldPathPortable(std::vector<std::string>{});
  }

  return FieldPathPortable(SplitOnDots(path));
}

/* static */
FieldPathPortable FieldPathPortable::KeyFieldPath() {
  return FieldPathPortable{
      std::vector<std::string>(1, FieldPathPortable::kDocumentKeyPath)};
}

}  // namespace firestore
}  // namespace firebase
