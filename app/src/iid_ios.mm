/*
 * Copyright 2017 Google LLC
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

#include "app/src/iid.h"

#import "FIRInstanceID.h"

namespace firebase {
namespace internal {

InstanceId::InstanceId(const App& app) : app_(app) {}

InstanceId::~InstanceId() {}

std::string InstanceId::GetMasterToken() const {
  NSString* token = [[FIRInstanceID instanceID] token];
  // TODO(b/65043712): The token will be unavailable upon first call after the app install if the
  // app does not use FCM. Calling token schedules a token fetch so subsequent calls will get a
  // valid token.
  if (token) {
    return token.UTF8String;
  }
  return "";
}

}  // namespace internal
}  // namespace firebase
