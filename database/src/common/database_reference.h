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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_DATABASE_REFERENCE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_DATABASE_REFERENCE_H_

#include "database/src/include/firebase/database/common.h"

namespace firebase {
namespace database {
namespace internal {

extern const char kErrorMsgConflictSetPriority[];
extern const char kErrorMsgConflictSetValue[];
extern const char kErrorMsgInvalidVariantForPriority[];
extern const char kErrorMsgInvalidVariantForUpdateChildren[];

// Future functions.
enum DatabaseReferenceFn {
  kDatabaseReferenceFnRemoveValue = 0,
  kDatabaseReferenceFnRunTransaction,
  kDatabaseReferenceFnSetValue,
  kDatabaseReferenceFnSetPriority,
  kDatabaseReferenceFnSetValueAndPriority,
  kDatabaseReferenceFnUpdateChildren,
  kDatabaseReferenceFnCount,
};

// Whether the variant is valid to be used for a priority.
// Fundamental types and server values are valid.
inline bool IsValidPriority(const Variant& v) {
  return (v.is_fundamental_type() || v == ServerTimestamp());
}

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_DATABASE_REFERENCE_H_
