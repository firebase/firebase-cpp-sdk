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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_CONNECTION_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_CONNECTION_H_

#include <sstream>

#include "app/memory/atomic.h"
#include "app/memory/unique_ptr.h"
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

// This is class combines the functionality of Connection.java and
// WebSocketConnection.java from Android/server SDK.
// The main purpose of this class:
// 1. Own a websocket client and keep track of connection state
// 2. Keep the connection alive
// 3. Break down each outgoing message to smaller frames
// 4. Combine incoming frames into full message
// 5. Parse each incoming message into a control message and data message
// 6. Handle control messages
// 7. Trigger useful events to higher level.
//
// Currently it does not automatically disconnect itself if it has never been
// used.  Also, it does not handle cache server.
//
// This class requires scheduler and expects all the public functions, except
// for events from WebSocketClientEventHandler, are called from scheduler's
// worker thread.
//
// This class is designed to be disposable and non-reusable.  That is, once
// disconnected, it is not able to reconnect again.  Simply create another
// Connection and open the connection again.
class Connection : public WebSocketClientEventHandler {
 public:
  enum DisconnectReason {
    // Disconnect due to the request from higher-level
    kDisconnectReasonManual = 0,
    // Disconnect due to errors in incoming wire protocol message
    kDisconnectReasonProtocolError,
    // Disconnect due to errors in web socket client
    kDisconnectReasonWebsocketError,
    // Disconnect due to connection lost after the connection is established.
    kDisconnectReasonConnectionLost,
    // Disconnect because unable to establish connection to the server
    kDisconnectReasonConnectionFailed,
    // Disconnect due to the shutdown message from the server
    kDisconnectReasonShutdownMessage,
    // Disconnect due to server reset
    kDisconnectReasonServerReset
  };

  explicit Connection(scheduler::Scheduler* scheduler, const HostInfo& info,
                      const char* opt_last_session_id,
                      ConnectionEventHandler* event_handler, Logger* logger);
  ~Connection() override;

  // Connection is neither copyable nor movable.
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  // Open the connection to firebase server, given host_info_.
  // Expect to be called from scheduler thread
  void Open();

  // Close the connection to firebase server.
  // Expect to be called from scheduler thread
  void Close(DisconnectReason reason = kDisconnectReasonManual);

  // Send a client data message to server in the format of Json.
  // { r: request-number, a: action, b: action-specific-body }
  // is_sensitive is used to determine whether the message would be printed to
  // the log.  Only Auth related message is sensitive.
  // Expect to be called from scheduler thread
  void Send(const Variant& message, bool is_sensitive);

  // BEGIN WebSocketClientEventHandler
  void OnOpen() override;
  void OnMessage(const char* msg) override;
  void OnClose() override;
  void OnError(const WebSocketClientErrorData& error_data) override;
  // END WebSocketClientEventHandler

 private:
  // State of the connection
  enum State {
    // Initial state, before Open() is called
    kStateNone = 0,
    // Once Open() is called and before the connection is ready
    kStateConnecting,
    // Once the websocket is connected and handshake message is received.
    kStateReady,
    // Final state once Close() is called.
    kStateDisconnected
  };

  // Combine incoming frames into one message, if the message is too large
  void HandleIncomingFrame(const char* msg);

  // Parse the message into data message or control message
  void ProcessMessage(const char* message);

  // Forward the data message to higher-level
  void OnDataMessage(const Variant& data);

  // Parse the control message
  void OnControlMessage(const Variant& data);

  // Handle control message to shutdown the connection
  void OnConnectionShutdown(const std::string& reason);

  // Handle hand-shake control message
  void OnHandshake(const Variant& handshake);

  // Once hand-shake is confirm, forward the session id and timestamp to higher
  // level.
  void OnConnectionReady(int64_t timestamp, const std::string& sessionId);

  // Handle reset control message
  void OnReset(const std::string& host);

  // Interval to send out "0" to server in order to keep the connection alive
  static const int kKeepAliveTimeoutMs;

  // Timeout for connection request to firebase server
  static const int kConnectTimeoutMs;

  // Maximum size of a frame for outgoing message
  static const int kMaxFrameSize;

  // Wire protocol keys and values
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

  // Log id.  Unique for each connection.
  static compat::Atomic<uint32_t> next_log_id_;
  std::string log_id_;

  // Safe reference to this.  Set in constructor and cleared in destructor
  // Should be safe to be copied in any thread because the SharedPtr never
  // changes, until safe_this_ is completely destroyed.
  typedef firebase::internal::SafeReference<Connection> ConnectionRef;
  typedef firebase::internal::SafeReferenceLock<Connection> ConnectionRefLock;
  ConnectionRef safe_this_;

  // Event handler for higher level
  ConnectionEventHandler* event_handler_;

  // Scheduler to make sure all WebSocketClient events are handled in worker
  // thread.
  scheduler::Scheduler* scheduler_;

  // Host info for websocket url
  const HostInfo host_info_;

  // Current connection state.  Only safe to access in scheduler thread
  State state_;

  // Whether websocket has been opened before.  Only safe to access in scheduler
  // thread
  bool ws_connected_;

  // Web socket client implementation.  Only safe to access in scheduler thread.
  UniquePtr<WebSocketClientInterface> client_;

  // The handle for periodic callback to keep the connection alive.  Only safe
  // to access in scheduler thread.
  scheduler::RequestHandle keep_alive_handler_;

  // Incoming message buffer
  std::stringstream incoming_buffer_;
  uint32_t expected_incoming_frames_;

  Logger* logger_;
};

// Event Handler interface for higher-level class to implement.
// All the function here will be triggered only from scheduler thread.
class ConnectionEventHandler {
 public:
  virtual ~ConnectionEventHandler() {}

  // Triggered when a handshake message or a reset message is received from
  // server  Those messages contain the information of the cache host.
  virtual void OnCacheHost(const std::string& host) = 0;

  // Triggered when the connection is ready to use.  session id can be used to
  // resume the same session with different Connection, if disconnected
  virtual void OnReady(int64_t timestamp, const std::string& session_id) = 0;

  // Triggered when a data message is received.
  virtual void OnDataMessage(const Variant& message) = 0;

  // Triggered when the connection is disconnected.
  virtual void OnDisconnect(Connection::DisconnectReason reason) = 0;

  // Triggered when the shutdown message is received from the server
  virtual void OnKill(const std::string& reason) = 0;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_CONNECTION_H_
