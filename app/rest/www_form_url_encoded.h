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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_WWW_FORM_URL_ENCODED_H_
#define FIREBASE_APP_CLIENT_CPP_REST_WWW_FORM_URL_ENCODED_H_

#include <string>
#include <vector>

namespace firebase {
namespace rest {

// Constructs and parses strings of key/value pairs in the
// x-www-form-urlencoded format.
// util::Initialize() must be called before this can be used.
class WwwFormUrlEncoded {
 public:
  // Form item.
  struct Item {
    Item() {}
    Item(const std::string& key_, const std::string& value_)
        : key(key_), value(value_) {}

    std::string key;
    std::string value;
  };

 public:
  // Initialize with a string to output constructed form data.
  explicit WwwFormUrlEncoded(std::string* output) : output_(output) {}

  // Add a key / value pair to the form.
  void Add(const char* key, const char* value);

  // Add a key / value pair using a form item.
  void Add(const Item& item) { Add(item.key.c_str(), item.value.c_str()); }

  // Get the form data string constructed by successive calls to Add().
  const std::string& form_data() const { return *output_; }

  // Parse a x-www-form-urlencoded encoded form into a list of key value pairs.
  static std::vector<Item> Parse(const char* form);

 private:
  std::string* output_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_WWW_FORM_URL_ENCODED_H_
