// Copyright 2020 Google LLC

#include "firestore/src/android/field_path_portable.h"

#include <cctype>
#include <cstring>
#include <sstream>

#include "firestore/src/common/exception_common.h"

namespace firebase::firestore {
namespace {

bool IsValidFieldPathSegment(const std::string& segment) {
  if (segment.empty()) return false;
  unsigned char first = static_cast<unsigned char>(segment[0]);
  if (first != '_' && !std::isalpha(first)) return false;
  for (size_t i = 1; i < segment.size(); ++i) {
    unsigned char ch = static_cast<unsigned char>(segment[i]);
    if (ch != '_' && !std::isalnum(ch)) return false;
  }
  return true;
}

std::string Escape(const std::string& segment) {
  if (IsValidFieldPathSegment(segment)) return segment;
  std::string result;
  result.reserve(segment.size() * 2 + 2);
  result.push_back('`');
  for (char c : segment) {
    if (c == '\\' || c == '`') result.push_back('\\');
    result.push_back(c);
  }
  result.push_back('`');
  return result;
}

std::vector<std::string> SplitOnDots(const std::string& input) {
  auto fail = [&input] {
    SimpleThrowInvalidArgument("Invalid field path (" + input +
                               "). Paths must not be empty, begin with '.', "
                               "end with '.', or contain '..'");
  };
  if (input.empty() || input.front() == '.' || input.back() == '.') {
    fail();
    return {};
  }
  std::vector<std::string> result;
  std::string seg;
  std::istringstream stream(input);
  while (std::getline(stream, seg, '.')) {
    if (seg.empty()) {
      fail();
      return {};
    }
    result.push_back(std::move(seg));
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
      SimpleThrowInvalidArgument("Invalid field name at index " +
                                 std::to_string(i) +
                                 ". Field names must not be empty.");
      return false;
    }
  }
  return true;
}

}  // namespace

std::string FieldPathPortable::CanonicalString() const {
  std::string result;
  for (size_t i = 0; i < segments_.size(); ++i) {
    if (i > 0) result.push_back('.');
    result.append(Escape(segments_[i]));
  }
  return result;
}

bool FieldPathPortable::IsKeyFieldPath() const {
  return size() == 1 && segments_[0] == kDocumentKeyPath;
}

FieldPathPortable FieldPathPortable::FromSegments(
    std::vector<std::string> segments) {
  return FieldPathPortable(ValidateSegments(segments)
                               ? std::move(segments)
                               : std::vector<std::string>{});
}

FieldPathPortable FieldPathPortable::FromDotSeparatedString(
    const std::string& path) {
  if (path.find_first_of("~*/[]") != std::string::npos) {
    SimpleThrowInvalidArgument("Invalid field path (" + path +
                               "). Paths must not contain '~', '*', '/', '[', "
                               "or ']'");
    return FieldPathPortable(std::vector<std::string>{});
  }
  return FieldPathPortable(SplitOnDots(path));
}

FieldPathPortable FieldPathPortable::KeyFieldPath() {
  return FieldPathPortable{std::vector<std::string>(1, kDocumentKeyPath)};
}

}  // namespace firebase::firestore
