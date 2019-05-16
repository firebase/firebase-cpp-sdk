/*
 * Copyright 2019 Google LLC
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

#include "app/rest/www_form_url_encoded.h"

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "app/rest/util.h"

namespace firebase {
namespace rest {

void WwwFormUrlEncoded::Add(const char* key, const char* value) {
  if (output_->length()) {
    *output_ += "&";
  }
  *output_ += util::EncodeUrl(key);
  *output_ += "=";
  *output_ += util::EncodeUrl(value);
}

std::vector<WwwFormUrlEncoded::Item> WwwFormUrlEncoded::Parse(
    const char* form) {
  std::vector<Item> form_data;
  const char* current_field = form;
  const char* end_of_form = form + strlen(form);
  while (current_field < end_of_form) {
    const char* end_of_field =
        std::find_if(current_field, end_of_form, [](char c) {
          return c == '&' || c == '\n' || c == '\r' || c == '\t' || c == ' ';
        });
    if (end_of_field != current_field) {
      const char* separator = std::find(current_field, end_of_field, '=');
      if (separator != end_of_field) {
        form_data.push_back(
            Item(util::DecodeUrl(std::string(current_field, separator)),
                 util::DecodeUrl(std::string(separator + 1, end_of_field))));
      }
    }
    current_field = end_of_field + 1;
  }
  return form_data;
}

}  // namespace rest
}  // namespace firebase
