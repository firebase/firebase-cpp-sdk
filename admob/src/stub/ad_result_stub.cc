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

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#include <string>

#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"

namespace firebase {
namespace admob {

struct AdResultInternal {
  int stub;
};

AdResult::AdResult(const AdResultInternal& ad_result_internal) {}

AdResult::~AdResult() {}

AdResult::AdResult(const AdResult& ad_result) {}

bool AdResult::is_successful() const { return false; }

std::unique_ptr<AdResult> AdResult::GetCause() {
  return std::unique_ptr<AdResult>(nullptr);
}

/// Gets the error's code.
int AdResult::code() { return 0; }

/// Gets the domain of the error.
const std::string& AdResult::domain() { return std::string(); }

/// Gets the message describing the error.
const std::string& AdResult::message() { return std::string(); }

/// Returns a log friendly string version of this object.
const std::string& AdResult::ToString() { return std::string(); }

}  // namespace admob
}  // namespace firebase
