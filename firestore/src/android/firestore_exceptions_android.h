/*
 * Copyright 2020 Google LLC
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

#ifndef FIRESTORE_FIRESTORE_EXCEPTIONS_ANDROID_H_
#define FIRESTORE_FIRESTORE_EXCEPTIONS_ANDROID_H_

// This file contains a Copy of the classes defined in
// Firestore/core/firestore_exceptions.h, without the absl dependency.

#include "Firestore/core/include/firebase/firestore/firestore_errors.h"
#include "firestore/src/common/macros.h"  // for FIRESTORE_HAVE_EXCEPTIONS

namespace firebase {
namespace firestore {

#if FIRESTORE_HAVE_EXCEPTIONS

/**
 * An exception thrown if Firestore encounters an unhandled error.
 */
class FirestoreException : public std::exception {
 public:
  FirestoreException(const std::string& message, Error code)
      : message_(message), code_(code) {}

  const char* what() const noexcept override { return message_.c_str(); }

  Error code() const { return code_; }

 private:
  std::string message_;
  Error code_;
};

/**
 * An exception thrown if Firestore encounters an internal, unrecoverable error.
 */
class FirestoreInternalError : public FirestoreException {
 public:
  FirestoreInternalError(const std::string& message,
                         Error code = Error::kErrorInternal)
      : FirestoreException(message, code) {}
};

#endif  // FIRESTORE_HAVE_EXCEPTIONS

}  // namespace firestore
}  // namespace firebase

#endif  // FIRESTORE_FIRESTORE_EXCEPTIONS_ANDROID_H_
