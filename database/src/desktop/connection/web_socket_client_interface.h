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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_INTERFACE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_INTERFACE_H_

#include <string>

namespace firebase {
namespace database {
namespace internal {
namespace connection {

// WebSocketClientInterface allows higher-level code to access lower-level
// functionalities of websocket independent of implementation, C++ version and
// platform.
class WebSocketClientInterface {
 public:
  virtual ~WebSocketClientInterface(){}

  // Request to connect to websocket server
  virtual void Connect(int timeout_ms) = 0;

  // Request to close established connection
  virtual void Close() = 0;

  // Request to send message to the connected server
  virtual void Send(const char* msg) = 0;
};

// Context when OnError occurs.  Currently only contains the uri.
class WebSocketClientErrorData {
 public:
  explicit WebSocketClientErrorData(const char* uri) : uri_(uri) {}

  const std::string& GetUri() const { return uri_; }

 private:
  WebSocketClientErrorData() : WebSocketClientErrorData("") {}

  std::string uri_;
};

class WebSocketClientEventHandler {
 public:
  virtual ~WebSocketClientEventHandler(){}

  // Called when the connection is established
  virtual void OnOpen() = 0;

  // Called when a message from the server is received
  virtual void OnMessage(const char* msg) = 0;

  // Called when the connection is closed
  virtual void OnClose() = 0;

  // Called when an error occurs while establishing the connection
  virtual void OnError(const WebSocketClientErrorData& error_data) = 0;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_INTERFACE_H_
