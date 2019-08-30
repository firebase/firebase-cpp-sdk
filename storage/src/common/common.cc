// Copyright 2016 Google LLC
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

#include "storage/src/include/firebase/storage/common.h"

#include <string.h>

#include "app/src/include/firebase/internal/common.h"
#include "storage/src/common/common_internal.h"
#include "storage/src/include/firebase/storage/metadata.h"

namespace firebase {
namespace storage {

static const char* g_error_messages[] = {
    // kErrorNone
    "The operation was a success, no error occurred",
    // kErrorUnknown
    "An unknown error occurred",
    // kErrorObjectNotFound
    "No object exists at the desired reference",
    // kErrorBucketNotFound
    "No bucket is configured for Cloud Storage",
    // kErrorProjectNotFound
    "No project is configured for Cloud Storage",
    // kErrorQuotaExceeded
    "Quota on your Cloud Storage bucket has been exceeded",
    // kErrorUnauthenticated
    "User is unauthenticated",
    // kErrorUnauthorized
    "User is not authorized to perform the desired action",
    // kErrorRetryLimitExceeded
    "The maximum time limit on an operation (upload, download, delete, etc.) "
    "has been exceeded",
    // kErrorNonMatchingChecksum
    "File on the client does not match the checksum of the file recieved by "
    "the server",
    // kErrorDownloadSizeExceeded
    "Size of the downloaded file exceeds the amount of memory allocated for "
    "the download",
    // kErrorCancelled
    "User cancelled the operation",
};

const char* GetErrorMessage(Error error) {
  return error < FIREBASE_ARRAYSIZE(g_error_messages) ? g_error_messages[error]
                                                      : "";
}

namespace internal {

// Set default fields for file uploads if they're not set.
void MetadataSetDefaults(Metadata* metadata) {
  // Content type to specify when metadata doesn't provided a content type.
  static const char kDefaultMetadataContentType[] = "application/octet-stream";

  // If the content type isn't set, use a default value.
  // This results in a valid content type being set on desktop and a consistent
  // value being used on iOS.
  // The iOS storage library sets the content-type field based upon filename
  // extension (when uploading from a file) vs.  Android which always sets this
  // to a default value.
  if (!metadata->content_type() || strlen(metadata->content_type()) == 0) {
    metadata->set_content_type(kDefaultMetadataContentType);
  }
}

}  // namespace internal

}  // namespace storage
}  // namespace firebase
