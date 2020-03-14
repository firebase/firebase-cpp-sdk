#include "firestore/src/android/field_path_portable.h"

#include <cctype>
#include <cstring>
#include <sstream>

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
  if (*iter != '_' && !std::isalpha(*iter)) {
    return false;
  }
  ++iter;
  while (iter != segment.end()) {
    if (*iter != '_' && !std::isalnum(*iter)) {
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
    FIREBASE_ASSERT_MESSAGE(
        false,
        "Invalid field path (%s). Paths must not be empty, begin with "
        "'.', end with '.', or contain '..'",
        input.c_str());
  };

  // `std::getline()` considers empty input to contain zero segments, and if
  // input ends with the separator followed by nothing, that is also not
  // considered a segment. Consequently, these cases are not covered by
  // validation in the body of the loop below and have to be checked beforehand.
  if (input.empty() || input[0] == '.' || input[input.size() - 1] == '.') {
    fail_validation();
  }

  std::vector<std::string> result;
  std::string current_segment;
  std::istringstream stream(input);

  while (std::getline(stream, current_segment, '.')) {
    if (current_segment.empty()) {
      fail_validation();
    }

    result.push_back(firebase::Move(current_segment));
  }

  return result;
}

}  // anonymous namespace

std::string FieldPathPortable::CanonicalString() const {
  std::vector<std::string> escaped_segments;
  escaped_segments.reserve(segments_.size());
  std::size_t length = 0;
  for (const std::string& segment : segments_) {
    escaped_segments.push_back(Escape(segment));
    length += escaped_segments.back().size() + 1;
  }
  if (length == 0) {
    return "";
  }

  std::string result;
  result.reserve(length);
  for (const std::string& segment : escaped_segments) {
    result.append(segment);
    result.push_back('.');
  }
  result.erase(result.end() - 1);
  return result;
}

bool FieldPathPortable::IsKeyFieldPath() const {
  return size() == 1 && segments_[0] == FieldPathPortable::kDocumentKeyPath;
}

FieldPathPortable FieldPathPortable::FromDotSeparatedString(
    const std::string& path) {
  FIREBASE_ASSERT_MESSAGE(
      path.find_first_of("~*/[]") == std::string::npos,
      "Invalid field path (%s). Paths must not contain '~', '*', '/', '[', "
      "or ']'",
      path.c_str());

  return FieldPathPortable(SplitOnDots(path));
}

/* static */
FieldPathPortable FieldPathPortable::FromServerFormat(const std::string& path) {
  // The following implementation is a STLPort-compatible version of code in
  //     Firestore/core/src/firebase/firestore/model/field_path.cc

  std::vector<std::string> segments;
  std::string segment;
  segment.reserve(path.size());

  // Inside backticks, dots are treated literally.
  bool inside_backticks = false;
  for (std::size_t i = 0; i < path.size(); ++i) {
    const char c = path[i];
    // std::string (and string_view) may contain embedded nulls. For full
    // compatibility with Objective C behavior, finish upon encountering the
    // first terminating null.
    if (c == '\0') {
      break;
    }

    switch (c) {
      case '.':
        if (!inside_backticks) {
          FIREBASE_ASSERT_MESSAGE(
              !segment.empty(),
              "Invalid field path (%s). Paths must not be empty, begin with "
              "'.', end with '.', or contain '..'",
              path.c_str());
          // Move operation will clear segment, but capacity will remain the
          // same (not, strictly speaking, required by the standard, but true in
          // practice).
          segments.push_back(firebase::Move(segment));
          segment.clear();
        } else {
          segment += c;
        }
        break;

      case '`':
        inside_backticks = !inside_backticks;
        break;

      case '\\':
        FIREBASE_ASSERT_MESSAGE(i + 1 != path.size(),
                                "Trailing escape characters not allowed in %s",
                                path.c_str());
        ++i;
        segment += path[i];
        break;

      default:
        segment += c;
        break;
    }
  }
  FIREBASE_ASSERT_MESSAGE(
      !segment.empty(),
      "Invalid field path (%s). Paths must not be empty, begin with "
      "'.', end with '.', or contain '..'",
      path.c_str());
  segments.push_back(firebase::Move(segment));

  FIREBASE_ASSERT_MESSAGE(!inside_backticks, "Unterminated ` in path %s",
                          path.c_str());

  return FieldPathPortable{firebase::Move(segments)};
}

/* static */
FieldPathPortable FieldPathPortable::KeyFieldPath() {
  return FieldPathPortable{
      std::vector<std::string>(1, FieldPathPortable::kDocumentKeyPath)};
}

}  // namespace firestore
}  // namespace firebase
