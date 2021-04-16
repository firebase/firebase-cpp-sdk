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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_HOST_INFO_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_HOST_INFO_H_
#include <string>

namespace firebase {
namespace database {
namespace internal {
namespace connection {

// Host info contains hostname, namespace and whether the connection should be
// secured.  It can compose http host url or websocket url.  For instance,
//   Hostname           = test.firebaseio.com
//   Namespace          = test
//   Secure             = true
//   Last Session       = ABC
//   ToString()         = https://test.firebaseio.com
//   GetConnectionUrl() = wss://test.firebaseio.com//.ws?ns=test&v=5&ls=ABC
// Note that the hostname may not start with namespace.  For instance, a cache
// server hostname may look like "s-usc1c-nss-123.firebaseio.com"
class HostInfo {
 public:
  explicit HostInfo() {}

  // Constructor to pass in hostname, namespace and whether this is for a
  // secured connection
  explicit HostInfo(const char* host, const char* ns, bool secure);
  HostInfo(const HostInfo& other);
  virtual ~HostInfo();

  HostInfo& operator=(const HostInfo& other);

  // Websocket connection url with session id
  std::string GetConnectionUrl(const char* last_session_id = nullptr) const;

  // Http host url
  std::string ToString() const;

  const std::string& GetHost() const { return host_; }

  const std::string& GetNamespace() const { return namespace_; }

  bool IsSecure() const { return secure_; }

  const std::string& user_agent() const { return user_agent_; }

  // User agent sent when a web socket connection is opened.
  const std::string& web_socket_user_agent() const {
    return web_socket_user_agent_;
  }

 private:
  // Key of Firebase Wire Protocol version.
  static const char* const kVersionParam;

  // Current Wire Protocol version number
  static const char* const kWireProtocolVersion;

  // Key of last session id.
  static const char* const kLastSessionIdParam;

  std::string host_;
  std::string namespace_;
  bool secure_;
  std::string user_agent_;
  std::string web_socket_user_agent_;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_HOST_INFO_H_
