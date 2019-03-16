// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_STORAGE_PATH_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_STORAGE_PATH_H_

#include <string>
#include "app/src/path.h"

namespace firebase {
namespace storage {
namespace internal {

extern const char kGsScheme[];

// Class for managing paths for firebase storage.
// Storage paths are made up of a bucket, a path,
// and (optionally) an object, located at that path.
class StoragePath {
 public:
  // Default constructor.
  StoragePath() {}

  // Constructs a storage path, based on an input URL.  The URL can either be
  // an HTTP[s] link, or a gs URI.
  explicit StoragePath(const std::string& path);

  // Constructs a storage path, based on raw strings for the bucket, path, and
  // object.
  StoragePath(const std::string& bucket, const std::string& path,
              const std::string& object = "");

  // The bucket portion of this path.
  // In the path: MyBucket/folder/object, it would return "MyBucket".
  const std::string& GetBucket() const { return bucket_; }

  const Path& GetPath() const { return path_; }

  // Returns the full path of the object.  This is the concatenation of the
  // bucket, local path, and object.  It's not a full URI, but it is the path
  // to the object, starting from the bucket.
  // ex:  "bucket/path1/path2/object"
  std::string GetFullPath() const { return bucket_ + kSeparator + path_.str(); }

  // Moves to a child of the current location.  If there is currently an object
  // specified, it is lost.  (i. e:
  // StoragePath("gs://bucket/path/object").Child("otherchild/") will result
  // in a path where bucket is "bucket", local_path is "path/otherchild/" and
  // object is an empty string.
  StoragePath GetChild(const std::string& path) const {
    return StoragePath(bucket_, path_.GetChild(path));
  }

  // Returns the location one folder up from the current location.  If the
  // path is at already at the root level, this returns the path unchanged.
  // The Object in the result is always set to empty.
  StoragePath GetParent() const {
    return StoragePath(bucket_, path_.GetParent());
  }

  // Returns the path as a HTTP URL to the asset.
  std::string AsHttpUrl() const;

  // Returns the path as a HTTP URL to the metadata for the asset.
  std::string AsHttpMetadataUrl() const;

  // Check to see if the path has been initialized correctly.
  bool IsValid() const { return !bucket_.empty(); }

 private:
  static const char* const kSeparator;

  StoragePath(const std::string& bucket, const Path& path)
      : bucket_(bucket), path_(path) {}

  void ConstructFromGsUri(const std::string& uri, int path_start);
  void ConstructFromHttpUrl(const std::string& url, int path_start);

  std::string bucket_;
  Path path_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_STORAGE_PATH_H_
