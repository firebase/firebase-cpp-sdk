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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_PATH_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_PATH_H_

#include <string>
#include <vector>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/optional.h"

namespace firebase {

// Class for managing paths for Firebase Database and Storage. Paths are made up
// of a forward-slash delimited list of strings.
class Path {
 public:
  // Default constructor.
  Path() : path_() {}

  // Constructs a path based on an input string, removing excess slashes.
  explicit Path(const std::string& path);

  // Construct a path based on a vector of strings, inserting slashes between
  // each directory.
  explicit Path(const std::vector<std::string>& directories);

  // Construct a path based on a vector iterators, inserting slashes between
  // each directory.
  Path(const std::vector<std::string>::iterator start,
       const std::vector<std::string>::iterator finish);

  bool operator==(const Path& other) const { return path_ == other.path_; }
  bool operator>=(const Path& other) const { return path_ >= other.path_; }
  bool operator>(const Path& other) const { return path_ > other.path_; }
  bool operator<=(const Path& other) const { return path_ <= other.path_; }
  bool operator<(const Path& other) const { return path_ < other.path_; }
  bool operator!=(const Path& other) const { return path_ != other.path_; }

  // Returns the full path of the object.
  const std::string& str() const { return path_; }

  // Returns the full path of the object as a c-style string.
  const char* c_str() const { return path_.c_str(); }

  // Returns true if this path is empty.
  bool empty() const { return path_.empty(); }

  // Create a new path at the child directory.
  Path GetChild(const std::string& child) const;

  // Create a new path at the child directory.
  Path GetChild(const Path& child_path) const;

  // Returns the location one folder up from the current location. If the
  // path is at already at the root level, this returns the path unchanged.
  Path GetParent() const;

  // The object that the path points to.
  // In the path: path/to/object/in/database, it would return "database".
  const char* GetBaseName() const;

  // Returns true if this path is the parent of the other path. The other path
  // is compared on a per-directory basis, not per-character. That is, for the
  // path "foo/bar/baz", the path "foo/bar" would be return true, but "foo/ba"
  // would return false.
  bool IsParent(const Path& other) const;

  // Returns a vector containing each directory in the path in order.
  // The path "foo/bar/baz" would return a vector containing "foo", "bar", and
  // "baz".
  std::vector<std::string> GetDirectories() const;

  // Returns the first directory in a path. If the path is empty then this
  // returns an empty path.
  // e.g. The path "foo/bar/baz" would return Path("foo").
  Path FrontDirectory() const;

  // Returns the path, omitting the first subdirectory. If the path is empty
  // then this returns an empty path.
  // e.g. The path "foo/bar/baz" would return Path("bar/baz").
  Path PopFrontDirectory() const;

  // Get the root path.
  static Path GetRoot() { return Path(); }

  // Given paths `from` and `to`, return the path from `from` to `to`. If a path
  // from A to B could be found, return true and place the result in out_result.
  // For example,
  //     Path result;
  //     Path from("first_star/on_left");
  //     Path to("first_star/on_left/straight_on/till_morning");
  //     Path::GetRelative(from, to, &result);
  //     assert(result.FullPath() == "straight_on/till_morning");
  static Optional<Path> GetRelative(const Path& from, const Path& to);

  FIREBASE_DEPRECATED static bool GetRelative(const Path& from, const Path& to,
                                              Path* out_result);

 private:
  static const char* const kSeparator;

  // Private contructor that skips the NormalizeSlashes for cases where we
  // know the slashes are correct.
  static Path MakePath(const std::string& path);

  // Removes any leading or trailing slashes, and collapses all consecutive
  // slashes into one.
  static std::string NormalizeSlashes(const std::string& path);

  std::string path_;
};

}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_PATH_H_
