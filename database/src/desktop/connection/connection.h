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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_CONNECTION_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_CONNECTION_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <sstream>

#include "app/src/include/firebase/variant.h"
#include "app/src/logger.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "database/src/desktop/connection/host_info.h"
#include "database/src/desktop/connection/web_socket_client_interface.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

class ConnectionEventHandler;

class Connection : public WebSocketClientEventHandler {
 public:
  enum DisconnectReason {
    kDisconnectReasonManual = 0,
    kDisconnectReasonProtocolError,
    kDisconnectReasonWebsocketError,
    kDisconnectReasonConnectionLost,
    kDisconnectReasonConnectionFailed,
    kDisconnectReasonShutdownMessage,
    kDisconnectReasonServerReset
  };

  explicit Connection(scheduler::Scheduler* scheduler, const HostInfo& info,
                      const char* opt_last_session_id,
                      ConnectionEventHandler* event_handler, Logger* logger,
                      const std::string& app_check_token = "");
  ~Connection() override;

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  void Open();
  void Close(DisconnectReason reason = kDisconnectReasonManual);
  void Send(const Variant& message, bool is_sensitive);

  void OnOpen() override;
  void OnMessage(const char* msg) override;
  void OnClose() override;
  void OnError(const WebSocketClientErrorData& error_data) override;

  void RefreshAppCheckToken(const std::string& token);

 private:
  enum State {
    kStateNone = 0,
    kStateConnecting,
    kStateReady,
    kStateDisconnected
  };

  void HandleIncomingFrame(const char* msg);
  void ProcessMessage(const char* message);
  void OnDataMessage(const Variant& data);
  void OnControlMessage(const Variant& data);
  void OnConnectionShutdown(const std::string& reason);
  void OnHandshake(const Variant& handshake);
  void OnConnectionReady(int64_t timestamp, const std::string& sessionId);
  void OnReset(const std::string& host);

  static const int kKeepAliveTimeoutMs;
  static const int kConnectTimeoutMs;
  static const int kMaxFrameSize;

  static const char* const kRequestType;
  static const char* const kRequestTypeData;
  static const char* const kRequestPayload;
  static const char* const kServerEnvelopeType;
  static const char* const kServerDataMessage;
  static const char* const kServerControlMessage;
  static const char* const kServerEnvelopeData;
  static const char* const kServerControlMessageType;
  static const char* const kServerControlMessageShutdown;
  static const char* const kServerControlMessageReset;
  static const char* const kServerControlMessageHello;
  static const char* const kServerControlMessageError;
  static const char* const kServerControlMessageData;
  static const char* const kServerHelloTimestamp;
  static const char* const kServerHelloHost;
  static const char* const kServerHelloSessionId;

  static std::atomic<uint32_t> next_log_id_;
  std::string log_id_;

  typedef firebase::internal::SafeReference<Connection> ConnectionRef;
  typedef firebase::internal::SafeReferenceLock<Connection> ConnectionRefLock;
  ConnectionRef safe_this_;

  ConnectionEventHandler* event_handler_;
  scheduler::Scheduler* scheduler_;
  const HostInfo host_info_;
  State state_;
  bool ws_connected_;
  std::unique_ptr<WebSocketClientInterface> client_;
  scheduler::RequestHandle keep_alive_handler_;
  std::stringstream incoming_buffer_;
  uint32_t expected_incoming_frames_;
  Logger* logger_;
};

class ConnectionEventHandler {
 public:
  virtual ~ConnectionEventHandler() {}
  virtual void OnCacheHost(const std::string& host) = 0;
  virtual void OnReady(int64_t timestamp, const std::string& session_id) = 0;
  virtual void OnDataMessage(const Variant& message) = 0;
  virtual void OnDisconnect(Connection::DisconnectReason reason) = 0;
  virtual void OnKill(const std::string& reason) = 0;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_CONNECTION_H_
