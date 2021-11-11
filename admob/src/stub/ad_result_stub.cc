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

#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"

namespace firebase {
namespace admob {

struct AdResultInternal {
  char stub;
};

AdResult::AdResult() {}

AdResult::AdResult(const AdResultInternal& ad_result_internal) {}

AdResult::AdResult(const AdResult& ad_result) : AdResult() {}

AdResult::~AdResult() {}

AdResult& AdResult::operator=(const AdResult& ad_result) { return *this; }

bool AdResult::is_successful() const { return false; }

std::unique_ptr<AdResult> AdResult::GetCause() const {
  return std::unique_ptr<AdResult>(nullptr);
}

/// Gets the error's code.
AdMobError AdResult::code() const { return kAdMobErrorNone; }

/// Gets the domain of the error.
const std::string& AdResult::domain() const {
  static const std::string empty;
  return empty;
}

/// Gets the message describing the error.
const std::string& AdResult::message() const {
  static const std::string empty;
  return empty;
}

/// Gets the ResponseInfo if an loadAd error occurred, with a collection of
/// information from each adapter.
const ResponseInfo& AdResult::response_info() const {
  static const ResponseInfo empty;
  return empty;
}

/// Returns a log friendly string version of this object.
const std::string& AdResult::ToString() const {
  static const std::string empty;
  return empty;
}

}  // namespace admob
}  // namespace firebase
