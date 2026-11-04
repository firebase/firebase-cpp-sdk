// Copyright 2018 Google LLC
#ifndef FIREBASE_STORAGE_SRC_COMMON_COMMON_INTERNAL_H_
#define FIREBASE_STORAGE_SRC_COMMON_COMMON_INTERNAL_H_

#include "storage/src/include/firebase/storage/metadata.h"

namespace firebase::storage::internal {
void MetadataSetDefaults(Metadata* metadata);
}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_COMMON_COMMON_INTERNAL_H_
