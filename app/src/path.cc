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

#include "app/src/path.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace firebase {

const char* const Path::kSeparator = "/";

// Utility function to split a string based on the input delimiters.
static std::vector<std::string> Split(const std::string& str,
                                      const char* delims) {
  std::vector<std::string> result;
  std::string::const_iterator finish;
  for (auto iter = str.begin(); iter != str.end(); iter = finish) {
    // Find next non-delimiter.
    auto start = std::find_if(iter, str.end(), [delims](char c) {
      return strchr(delims, c) == nullptr;
    });
    // Find next delimiter.
    finish = std::find_if(start, str.end(), [delims](char c) {
      return strchr(delims, c) != nullptr;
    });
    if (start != finish) {
      result.push_back(std::string(start, finish));
    }
  }
  return result;
}

// Utility function to join a vector of strings, separated by the separator
// string. This version takes just the vector iterators. This is often used in
// conjuntion with the `GetDirectories()` function, while iterating over the
// directories that make up a path.
static std::string Join(const char* separator,
                        std::vector<std::string>::iterator start,
                        std::vector<std::string>::iterator finish) {
  std::string result;

  // Starting and ending at the same place results in an empty path.
  int iterator_distance = std::distance(start, finish);
  if (iterator_distance == 0) return result;

  // Reserve space to avoid unnecessary allocations.
  int separator_count = iterator_distance - 1;
  int separator_length = std::strlen(separator);
  int joined_strings_length = 0;
  for (auto iter = start; iter != finish; ++iter) {
    joined_strings_length += iter->size();
  }
  int total_string_length =
      (separator_count * separator_length) + joined_strings_length;
  result.reserve(total_string_length);

  // Add the strings with separators.
  bool first_pass = true;
  for (auto iter = start; iter != finish; ++iter) {
    // Only add the separator after the first pass.
    if (!first_pass) result += separator;
    first_pass = false;

    result += *iter;
  }
  return result;
}

// Utility function to join a vector of strings, separated by the separator
// string.
static std::string Join(const char* separator,
                        std::vector<std::string> strings) {
  return Join(separator, strings.begin(), strings.end());
}

// Utility function to join a vector of strings, separated by the separator
// string.
static std::string Join(const std::string& separator,
                        std::vector<std::string> strings) {
  return Join(separator.c_str(), strings.begin(), strings.end());
}

Path::Path(const std::string& path) : path_(NormalizeSlashes(path)) {}

Path::Path(const std::vector<std::string>& directories)
    : Path(Join(kSeparator, directories)) {}

Path::Path(const std::vector<std::string>::iterator start,
           const std::vector<std::string>::iterator finish)
    : Path(Join(kSeparator, start, finish)) {}

Path Path::GetChild(const std::string& child) const {
  return Path(path_ + kSeparator + child);
}

Path Path::GetChild(const Path& child_path) const {
  return Path(path_ + kSeparator + child_path.path_);
}

Path Path::GetParent() const {
  std::string::size_type index = path_.find_last_of(kSeparator);
  // Reached the end without finding separator character. Return empty path.
  if (index == std::string::npos) {
    return Path();
  }
  return MakePath(path_.substr(0, index));
}

const char* Path::GetBaseName() const {
  std::string::size_type index = path_.find_last_of(kSeparator);
  // If there was no slash, either this is a single directory path or an empty
  // path. In either case, just return that.
  if (index == std::string::npos) {
    return path_.c_str();
  }
  return path_.c_str() + index + 1;
}

bool Path::IsParent(const Path& other) const {
  if (empty()) return true;
  if (path_.size() > other.path_.size()) return false;
  auto other_iter = other.path_.begin();
  auto this_iter = path_.begin();
  for (; other_iter != other.path_.end() && this_iter != path_.end();
       ++other_iter, ++this_iter) {
    if (*other_iter != *this_iter) break;
  }
  return other_iter == other.path_.end() || *other_iter == '/';
}

std::vector<std::string> Path::GetDirectories() const {
  return Split(path_, kSeparator);
}

Path Path::FrontDirectory() const {
  if (empty()) return Path();
  return Path(GetDirectories().front());
}

Path Path::PopFrontDirectory() const {
  if (empty()) return Path();
  std::vector<std::string> directories = GetDirectories();
  auto iter = directories.begin();
  return Path(++iter, directories.end());
}

bool Path::GetRelative(const Path& from, const Path& to, Path* out_result) {
  Optional<Path> result = GetRelative(from, to);
  if (result.has_value()) {
    *out_result = result.value();
    return true;
  } else {
    return false;
  }
}

Optional<Path> Path::GetRelative(const Path& from, const Path& to) {
  std::vector<std::string> from_dirs = from.GetDirectories();
  std::vector<std::string> to_dirs = to.GetDirectories();

  // Iterate over each subdirectory, ensuring that they are all equal.
  auto to_iter = to_dirs.begin();
  auto from_iter = from_dirs.begin();
  for (; from_iter != from_dirs.end() && to_iter != to_dirs.end();
       ++from_iter, ++to_iter) {
    if (*from_iter != *to_iter) {
      // Found an inequality; there is no path from `from` to `to`.
      return Optional<Path>();
    }
  }
  // Ensure that the from path reached the end.
  if (from_iter != from_dirs.end()) {
    return Optional<Path>();
  }
  // Join what remains of the `to` path, and return true.
  return Optional<Path>(MakePath(Join(kSeparator, to_iter, to_dirs.end())));
}

Path Path::MakePath(const std::string& path) {
  Path result;
  // Set the path_ directly, skipping the NormalizePaths step.
  result.path_ = path;
  return result;
}

std::string Path::NormalizeSlashes(const std::string& path) {
  std::string result;
  std::string::const_iterator finish;
  bool first_pass = true;
  for (auto iter = path.begin(); iter != path.end(); iter = finish) {
    // Find next non-slash.
    auto start = std::find_if(iter, path.end(), [](char c) {
      return strchr(kSeparator, c) == nullptr;
    });
    // Find next slash.
    finish = std::find_if(start, path.end(), [](char c) {
      return strchr(kSeparator, c) != nullptr;
    });

    // Append the string (and separator).
    if (start != finish) {
      if (!first_pass) result += kSeparator;
      first_pass = false;
      result.insert(result.end(), start, finish);
    }
  }
  return result;
}

}  // namespace firebase
