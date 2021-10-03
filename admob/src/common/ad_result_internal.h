/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_ADMOB_SRC_COMMON_AD_RESULT_INTERNAL_H_
#define FIREBASE_ADMOB_SRC_COMMON_AD_RESULT_INTERNAL_H_

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/types.h"

namespace firebase {
namespace admob {
namespace internal {

class AdResultInternal {
 public:
  /// Returns true if the Ad Result was successfull.
  virtual bool is_successful() const = 0;

  /// Retrieves an AdResult which represents the cause of this error.
  ///
  /// @return an AdResult which represents the cause of this AdResult.
  /// If there was no cause then the methods of the result will return
  /// defaults.
  virtual AdResult GetCause() const = 0;

  /// Gets the error's code.
  virtual int code() const = 0;

  /// Gets the domain of the error.
  virtual const std::string& domain() const = 0;

  /// Gets the message describing the error.
  virtual const std::string& message() const = 0;

  /// Returns a log friendly string version of this object.
  virtual const std::string& ToString() const = 0;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_COMMON_AD_RESULT_INTERNAL_H_
