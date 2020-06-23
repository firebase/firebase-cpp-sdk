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

#ifndef FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_FAKES_H_
#define FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_FAKES_H_

#include <string>
#include <unordered_map>

// A set of helpers to reduce repetitive boilerplate to setup fakes in tests.

namespace firebase {
namespace auth {
namespace test {

using FakeSetT = std::unordered_map<std::string, std::string>;

// Creates a JSON string from the given map of fakes (which assumes a very
// simple format, both keys and values can only be strings).
std::string CreateRawJson(const FakeSetT& fakes);

// Creates a JSON string from the given map of fakes and initializes Firebase
// testing config with this JSON.
void InitializeConfigWithFakes(const FakeSetT& fakes);

// Creates JSON dictionary with just a single entry (key = url, value
// = fake_response) and initializes Firebase testing config with this JSON.
void InitializeConfigWithAFake(const std::string& url,
                               const std::string& fake_response);

// Returns full URL to make a REST request to Identity Toolkit backend.
std::string GetUrlForApi(const std::string& api_key,
                         const std::string& api_method);

// Returns string representation of a successful HTTP response with the given
// body.
std::string FakeSuccessfulResponse(const std::string& body);

// Returns string representation of a successful HTTP response with the given
// body. Body will also contain an entry to specify the "kind" of response, like
// all Identity Toolkit responses do ("kind":
// "identitytoolkit#<YOUR_KIND_HERE>").
std::string FakeSuccessfulResponse(const std::string& kind,
                                   const std::string& body);

// Returns string representation of a 503 HTTP response.
std::string CreateErrorHttpResponse(const std::string& error = "");

}  // namespace test
}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_FAKES_H_
