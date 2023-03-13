/*
 * Copyright 2021 Google LLC
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

#include <string>

#include "gma/src/common/ad_error_internal.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

const char* const AdError::kUndefinedDomain = "undefined";

AdError::AdError() {}

AdError::AdError(const AdErrorInternal& ad_error_internal) {}

AdError::AdError(const AdError& ad_error) : AdError() {}

AdError::~AdError() {}

AdError& AdError::operator=(const AdError& obj) { return *this; }

std::unique_ptr<AdError> AdError::GetCause() const {
  return std::unique_ptr<AdError>(nullptr);
}

/// Gets the error's code.
AdErrorCode AdError::code() const { return kAdErrorCodeNone; }

/// Gets the domain of the error.
const std::string& AdError::domain() const {
  static const std::string empty;
  return empty;
}

/// Gets the message describing the error.
const std::string& AdError::message() const {
  static const std::string empty;
  return empty;
}

/// Gets the ResponseInfo if an loadAd error occurred, with a collection of
/// information from each adapter.
const ResponseInfo& AdError::response_info() const {
  static const ResponseInfo empty;
  return empty;
}

/// Returns a log friendly string version of this object.
const std::string& AdError::ToString() const {
  static const std::string empty;
  return empty;
}

}  // namespace gma
}  // namespace firebase
