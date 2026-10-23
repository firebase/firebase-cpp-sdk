// Copyright 2026 Google LLC
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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_HUB_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_HUB_H_

#include <string>

#include "app/src/logger.h"
#include "uWebSockets/src/uWS.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

// Returns the host component of a WebSocket URI, for example
//   "wss://test.firebaseio.com/.ws?ns=test" -> "test.firebaseio.com"
//   "ws://localhost:9000"                   -> "localhost"
//   "wss://[::1]:9000/"                     -> "::1"
// Any userinfo, port, path, query and fragment are discarded. Returns an empty
// string if the URI does not contain a host.
std::string GetHostFromWebSocketUri(const std::string& uri);

// A uWS::Hub whose outgoing wss:// connections authenticate the server.
//
// uWebSockets creates its client SSL_CTX with certificate verification
// disabled, so a plain uWS::Hub would complete a TLS handshake with any
// certificate, including self-signed ones or ones issued to a different host.
// WebSocketHub configures that SSL_CTX so that every secure connection it
// opens:
//   * negotiates TLS 1.2 or newer,
//   * presents a certificate chain that validates against the operating
//     system's trusted root store, and
//   * presents a leaf certificate that matches `host` (RFC 6125).
//
// A connection that fails any of these checks is aborted during the TLS
// handshake and surfaces through Hub::onError, exactly like a TCP connection
// failure. The reason is logged through `logger`.
//
// The trusted roots are read once per process from the platform:
//   * Windows: the "ROOT" certificate store (machine, group policy and
//     enterprise roots included).
//   * macOS: Apple's system roots plus roots explicitly trusted in the admin
//     or user trust settings.
//   * Linux and other POSIX: the distribution's CA bundle and hashed
//     certificate directory, plus the SSL_CERT_FILE / SSL_CERT_DIR
//     environment variables.
//
// This only affects connections opened with Hub::connect(). It has no effect
// on plaintext ws:// connections or on server sockets created with
// Hub::listen().
//
// uWS::Hub has no virtual destructor, so this class is final and must be held
// by value or by WebSocketHub pointer, never deleted through a uWS::Hub*.
class WebSocketHub final : public uWS::Hub {
 public:
  // `host` is the host name (or IP literal) that this Hub will connect to; the
  // server's certificate must be valid for it. `logger` is used to report
  // rejected certificates and must outlive this object.
  WebSocketHub(const std::string& host, Logger* logger);

  // WebSocketHub is neither copyable nor movable.
  WebSocketHub(const WebSocketHub&) = delete;
  WebSocketHub& operator=(const WebSocketHub&) = delete;

  // Runs the event loop on the calling thread, like uS::Node::run(), after
  // recording that thread as the loop thread everywhere uWebSockets consults
  // it. See the definition for why this is needed.
  void run();

  const std::string& host() const { return host_; }
  Logger* logger() const { return logger_; }

  // The SSL_CTX that Hub::connect() creates every wss:// connection from,
  // configured as described above. Exposed so tests can drive handshakes
  // against it; production code has no reason to touch it.
  SSL_CTX* client_ssl_context() const { return nodeData->clientContext; }

 private:
  const std::string host_;
  Logger* const logger_;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_HUB_H_
