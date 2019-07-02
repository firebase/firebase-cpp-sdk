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

#include "app/rest/transport_mock.h"
#include <string>
#include "app/rest/util.h"
#include "testing/config_desktop.h"

namespace firebase {
namespace rest {
void TransportMock::PerformInternal(
    Request* request, Response* response,
    flatbuffers::unique_ptr<Controller>* controller_out) {
  const char* url = request->options().url.c_str();
  // Naively use the whole request url as a key.
  const firebase::testing::cppsdk::ConfigRow* row =
      firebase::testing::cppsdk::ConfigGet(url);

  // Not specified in the test config. Returns status 404 (not found).
  if (row == nullptr || row->httpresponse() == nullptr) {
    // The status line for 404 not found. Ideally we could use the same HTTP
    // version as in the request; but that does not matter much and add code
    // complexity.
    const char* kHttp404Status = "HTTP/1.1 404 Not Found\r\n";
    response->ProcessHeader(kHttp404Status, strlen(kHttp404Status));
    response->ProcessHeader(util::kCrLf, strlen(util::kCrLf));
    response->MarkCompleted();
    return;
  }

  // Process header
  if (row->httpresponse()->header()) {
    for (const auto* header : *row->httpresponse()->header()) {
      const std::string& trimmed_header = util::TrimWhitespace(header->str());
      if (trimmed_header.empty()) {
        // Empty line in header marks the end of header. We break the for-loop.
        break;
      }
      const std::string& canonical_header = trimmed_header + util::kCrLf;
      if (!response->ProcessHeader(canonical_header.c_str(),
                                   canonical_header.size())) {
        response->MarkCompleted();
        return;
      }
    }
  }
  response->ProcessHeader(util::kCrLf, strlen(util::kCrLf));

  // Process body
  if (row->httpresponse()->body()) {
    for (const auto* body : *row->httpresponse()->body()) {
      if (!response->ProcessBody(body->c_str(), body->size())) {
        response->MarkCompleted();
        return;
      }
    }
  }

  // Mark completed
  response->MarkCompleted();
}
}  // namespace rest
}  // namespace firebase
