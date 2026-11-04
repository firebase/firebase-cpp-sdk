// Copyright 2017 Google LLC

#include "storage/src/desktop/storage_path.h"

#include <string>
#include <string_view>

#include "app/rest/util.h"
#include "storage/src/desktop/storage_desktop.h"

namespace firebase::storage::internal {
namespace {

constexpr std::string_view kHttpScheme = "http://";
constexpr std::string_view kHttpsScheme = "https://";
constexpr std::string_view kBucketEndString = "/o/";
constexpr std::string_view kBucketIdentifierString = "/v0/b/";
constexpr std::string_view kBucketOnlySuffix = "/o";

constexpr bool StartsWith(std::string_view str, std::string_view prefix) {
  return str.substr(0, prefix.size()) == prefix;
}
constexpr bool EndsWith(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size() &&
         str.substr(str.size() - suffix.size()) == suffix;
}

}  // namespace

const char* const StoragePath::kSeparator = "/";
const char kGsScheme[] = "gs://";

StoragePath::StoragePath(StorageInternal* storage, const std::string& path)
    : storage_internal_(storage) {
  const std::string_view uri(path);
  const std::string_view gs_scheme(kGsScheme);
  if (StartsWith(uri, gs_scheme)) {
    ConstructFromGsUri(uri.substr(gs_scheme.size()));
  } else if (StartsWith(uri, kHttpScheme)) {
    ConstructFromHttpUrl(uri.substr(kHttpScheme.size()));
  } else if (StartsWith(uri, kHttpsScheme)) {
    ConstructFromHttpUrl(uri.substr(kHttpsScheme.size()));
  }
}

StoragePath::StoragePath(StorageInternal* storage, const std::string& bucket,
                         const std::string& path, const std::string& object)
    : bucket_(bucket),
      path_(Path(path).GetChild(object)),
      storage_internal_(storage) {}

void StoragePath::ConstructFromGsUri(std::string_view bucket_and_path) {
  const auto first_slash_pos = bucket_and_path.find('/');
  bucket_ = std::string(bucket_and_path.substr(0, first_slash_pos));
  path_ = Path(first_slash_pos == std::string_view::npos
                   ? std::string()
                   : std::string(bucket_and_path.substr(first_slash_pos)));
}

void StoragePath::ConstructFromHttpUrl(std::string_view url) {
  const std::string_view path = url.substr(0, url.find_first_of("?#"));
  auto bucket_start = path.find(kBucketIdentifierString);
  if (bucket_start == std::string_view::npos || bucket_start == 0) return;
  bucket_start += kBucketIdentifierString.size();

  auto bucket_end = path.find(kBucketEndString, bucket_start);
  std::string_view object_encoded;
  if (bucket_end != std::string_view::npos) {
    object_encoded = path.substr(bucket_end + kBucketEndString.size());
  } else if (path.size() >= bucket_start + kBucketOnlySuffix.size() &&
             EndsWith(path, kBucketOnlySuffix)) {
    bucket_end = path.size() - kBucketOnlySuffix.size();
  } else {
    bucket_end = path.find('/', bucket_start);
    if (bucket_end == std::string_view::npos) {
      bucket_end = path.size();
    } else if (bucket_end + 1 < path.size()) {
      return;
    }
  }

  const std::string_view parsed_bucket =
      path.substr(bucket_start, bucket_end - bucket_start);
  if (parsed_bucket.empty() || parsed_bucket.find('/') != std::string_view::npos) {
    return;
  }
  bucket_ = std::string(parsed_bucket);
  path_ = Path(rest::util::DecodeUrl(std::string(object_encoded)));
}

std::string StoragePath::AsHttpUrl() const {
  return storage_internal_ ? AsHttpMetadataUrl().append("?alt=media") : "";
}

std::string StoragePath::AsHttpMetadataUrl() const {
  if (!storage_internal_) return "";
  return storage_internal_->get_scheme() + "://" +
         storage_internal_->get_host() + ":" +
         std::to_string(storage_internal_->get_port()) +
         std::string(kBucketIdentifierString) + bucket_ +
         std::string(kBucketEndString) + rest::util::EncodeUrl(path_.str());
}

}  // namespace firebase::storage::internal
