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

#include "auth/src/desktop/get_additional_user_info.h"

#include "app/src/log.h"
#include "app/src/variant_util.h"

namespace firebase {
namespace auth {
namespace detail {

std::map<Variant, Variant> ParseUserProfile(const std::string& json) {
  if (json.empty()) {
    return std::map<Variant, Variant>();
  }

  const Variant parsed_profile = firebase::util::JsonToVariant(json.c_str());
  if (parsed_profile.is_map()) {
    return parsed_profile.map();
  }

  LogError(
      "Expected user profile from server response to contain map as the root "
      "element");
  return std::map<Variant, Variant>();
}

}  // namespace detail
}  // namespace auth
}  // namespace firebase
