// Copyright 2016 Google LLC
#include "storage/src/include/firebase/storage/common.h"

#include <cstring>

#include "app/src/include/firebase/internal/common.h"
#include "storage/src/common/common_internal.h"
#include "storage/src/include/firebase/storage/metadata.h"

namespace firebase::storage {

static const char* const g_error_messages[] = {
    "The operation was a success, no error occurred",
    "An unknown error occurred",
    "No object exists at the desired reference",
    "No bucket is configured for Cloud Storage",
    "No project is configured for Cloud Storage",
    "Quota on your Cloud Storage bucket has been exceeded",
    "User is unauthenticated",
    "User is not authorized to perform the desired action",
    "The maximum time limit on an operation (upload, download, delete, etc.) has been exceeded",
    "File on the client does not match the checksum of the file received by the server",
    "Size of the downloaded file exceeds the amount of memory allocated for the download",
    "User cancelled the operation",
    "An invalid argument was provided",
};

const char* GetErrorMessage(Error error) {
  return (error >= 0 &&
          static_cast<size_t>(error) < FIREBASE_ARRAYSIZE(g_error_messages))
             ? g_error_messages[error]
             : "Unknown error.";
}

namespace internal {
void MetadataSetDefaults(Metadata* metadata) {
  if (!metadata || !metadata->is_valid()) return;
  if (!metadata->content_type() || std::strlen(metadata->content_type()) == 0) {
    metadata->set_content_type("application/octet-stream");
  }
}
}  // namespace internal

}  // namespace firebase::storage
