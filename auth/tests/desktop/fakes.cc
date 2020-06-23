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

#include "auth/tests/desktop/fakes.h"

#include "testing/config.h"

namespace firebase {
namespace auth {
namespace test {

std::string CreateRawJson(const FakeSetT& fakes) {
  std::string raw_json =
      "{"
      "  config:"
      "    [";

  for (auto i = fakes.begin(); i != fakes.end(); ++i) {
    const std::string url = i->first;
    const std::string response = i->second;
    raw_json +=
        "      {"
        "        fake: '" +
        url +
        "',"
        "        httpresponse: " +
        response + "      }";
    auto check_end = i;
    ++check_end;
    if (check_end != fakes.end()) {
      raw_json += ',';
    }
  }

  raw_json +=
      "    ]"
      "}";

  return raw_json;
}

void InitializeConfigWithFakes(const FakeSetT& fakes) {
  firebase::testing::cppsdk::ConfigSet(CreateRawJson(fakes).c_str());
}

void InitializeConfigWithAFake(const std::string& url,
                               const std::string& fake_response) {
  FakeSetT fakes;
  fakes[url] = fake_response;
  InitializeConfigWithFakes(fakes);
}

std::string GetUrlForApi(const std::string& api_key,
                         const std::string& api_method) {
  const char* const base_url =
      "https://www.googleapis.com/identitytoolkit/v3/"
      "relyingparty/";
  return std::string{base_url} + api_method + "?key=" + api_key;
}

std::string FakeSuccessfulResponse(const std::string& body) {
  const std::string head =
      "{"
      "  header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "  body: "
      "    ["
      "      '{";

  const std::string tail =
      "       }'"
      "    ]"
      "}";

  return head + body + tail;
}

std::string FakeSuccessfulResponse(const std::string& kind,
                                   const std::string& body) {
  return FakeSuccessfulResponse("\"kind\": \"identitytoolkit#" + kind + "\"," +
                                body);
}

std::string CreateErrorHttpResponse(const std::string& error) {
  const std::string head =
      "{"
      "  header: ['HTTP/1.1 503 Service Unavailable','Server:mock 101']";

  std::string body;
  if (!error.empty()) {
    // clang-format off
    body = std::string(
        ","
        "  body: ['{"
        "    \"error\": {"
        "      \"message\": \"") + error + "\""
        "    }"
        "  }']";
    // clang-format on
  }

  const std::string tail = "}";
  return head + body + tail;
}

}  // namespace test
}  // namespace auth
}  // namespace firebase
