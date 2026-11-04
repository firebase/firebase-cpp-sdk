// Copyright 2017 Google LLC

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_PATH_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_PATH_H_

#include <string>
#include <string_view>

#include "app/src/path.h"

namespace firebase::storage::internal {

extern const char kGsScheme[];
class StorageInternal;

class StoragePath {
 public:
  StoragePath() = default;
  explicit StoragePath(StorageInternal* storage, const std::string& path);
  StoragePath(StorageInternal* storage, const std::string& bucket,
              const std::string& path, const std::string& object = "");

  const std::string& GetBucket() const { return bucket_; }
  const Path& GetPath() const { return path_; }
  std::string GetFullPath() const { return bucket_ + kSeparator + path_.str(); }
  StoragePath GetChild(const std::string& path) const {
    return StoragePath(storage_internal_, bucket_, path_.GetChild(path));
  }
  StoragePath GetParent() const {
    return StoragePath(storage_internal_, bucket_, path_.GetParent());
  }
  std::string AsHttpUrl() const;
  std::string AsHttpMetadataUrl() const;
  bool IsValid() const { return !bucket_.empty(); }

 private:
  static const char* const kSeparator;

  StoragePath(StorageInternal* storage, const std::string& bucket, const Path& path)
      : bucket_(bucket), path_(path), storage_internal_(storage) {}

  void ConstructFromGsUri(std::string_view bucket_and_path);
  void ConstructFromHttpUrl(std::string_view url);

  std::string bucket_;
  Path path_;
  StorageInternal* storage_internal_ = nullptr;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_PATH_H_
