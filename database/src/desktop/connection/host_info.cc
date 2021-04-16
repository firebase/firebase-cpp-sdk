// Copyright 2018 Google LLC
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

#include "database/src/desktop/connection/host_info.h"

#include <assert.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "app/src/app_common.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

const char* const HostInfo::kVersionParam = "v";
const char* const HostInfo::kWireProtocolVersion = "5";
const char* const HostInfo::kLastSessionIdParam = "ls";

HostInfo::HostInfo(const char* host, const char* ns, bool secure)
    : host_(host), namespace_(ns), secure_(secure) {
  std::string sdk;
  std::string version;
  app_common::GetOuterMostSdkAndVersion(&sdk, &version);
  assert(!sdk.empty() && !version.empty());
  std::string os = std::string(app_common::kOperatingSystem);
  // User Agent Format:
  // Firebase/<wire_protocol>/<sdk_version>/<platform>/<device>
  user_agent_ = std::string("Firebase/") + std::string(kWireProtocolVersion) +
                "/" + version + "/" + sdk + "/" + os;

  // When the connection is established via web sockets, the client can send
  // the SDK version to the server to be logged.  This is in the format:
  // sdk.<platform>.<hypen_separated_sdk_version>
  // e.g sdk.cpp.1-2-3-windows
  std::string hypen_separated_sdk_version = version + std::string("-") + os;
  std::replace(hypen_separated_sdk_version.begin(),
               hypen_separated_sdk_version.end(), '.', '-');
  web_socket_user_agent_ = std::string("sdk.") + sdk + std::string(".") +
                           hypen_separated_sdk_version;
}

HostInfo::HostInfo(const HostInfo& other) { *this = other; }

HostInfo::~HostInfo() {}

HostInfo& HostInfo::operator=(const HostInfo& other) {
  host_ = other.host_;
  namespace_ = other.namespace_;
  secure_ = other.secure_;
  user_agent_ = other.user_agent_;
  web_socket_user_agent_ = other.web_socket_user_agent_;
  return *this;
}

std::string HostInfo::GetConnectionUrl(const char* last_session_id) const {
  std::stringstream ss;
  ss << (secure_ ? "wss" : "ws");
  ss << "://" << host_ << "/.ws?ns=" << namespace_;
  ss << '&' << kVersionParam << '=' << kWireProtocolVersion;
  if (last_session_id != nullptr) {
    ss << "&" << kLastSessionIdParam << "=" << last_session_id;
  }
  return ss.str();
}

std::string HostInfo::ToString() const {
  std::stringstream ss;
  ss << "http" << (secure_ ? "s" : "") << "://" << host_;
  return ss.str();
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
