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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include "app/rest/curl/curl_handles.h"
#include "app/rest/curl/curl_io_loop.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/logger.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "curl/curl.h"
#include "database/src/desktop/connection/web_socket_client_interface.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

// libcurl-based WebSocket client running on the SDK's shared I/O thread.
//
// Threading model
// ---------------
// * There is no thread per client. All socket I/O - the HTTP Upgrade
//   handshake (a curl::Transfer on the process-wide rest::curl::CurlIoLoop)
//   and, once open, curl_ws_recv()/curl_ws_send() on the CURLOPT_CONNECT_ONLY
//   socket registered with CurlIoLoop::AddExtraFd() - happens on that single
//   shared I/O thread, which also services every REST transfer in the process.
// * Connect(), Close(), Send() and the destructor may be called from any
//   thread (in practice the Database scheduler thread). They post work to the
//   I/O thread and return immediately; the destructor posts a teardown and
//   waits for it, so no I/O-thread work touches this object after it returns.
// * WebSocketClientEventHandler callbacks (OnOpen/OnMessage/OnClose/OnError)
//   are never invoked on the I/O thread: they are marshalled to the Database
//   `scheduler_` through `safe_this_`, which also makes them no-ops after the
//   client is destroyed. The Database therefore never blocks the I/O thread,
//   and a slow Database never stalls other products' network traffic.
// * The connection holds a rest::InitTransportCurl() reference for its whole
//   lifetime, which is what keeps the shared loop alive.
class WebSocketClientImpl : public WebSocketClientInterface,
                            private firebase::rest::curl::Transfer {
 public:
  WebSocketClientImpl(const std::string& uri, const std::string& user_agent,
                      Logger* logger, scheduler::Scheduler* scheduler,
                      const std::string& app_check_token,
                      WebSocketClientEventHandler* handler = nullptr);
  ~WebSocketClientImpl() override;

  // WebSocketClientImpl is neither copyable nor movable.
  WebSocketClientImpl(const WebSocketClientImpl&) = delete;
  WebSocketClientImpl& operator=(const WebSocketClientImpl&) = delete;

  // BEGIN WebSocketClientInterface
  void Connect(int timeout_ms) override;
  void Close() override;
  void Send(const char* msg) override;
  // END WebSocketClientInterface

  // Refresh the stored App Check token being used by the connection.
  // This doesn't change the connection itself, just the data used for
  // establishing new connections.
  // Expect to be called from scheduler thread.
  void RefreshAppCheckToken(const std::string& token);

 private:
  enum State {
    kStateIdle = 0,
    kStateConnecting,
    kStateOpen,
    kStateClosed,
  };

  // rest::curl::Transfer - the HTTP Upgrade handshake. I/O thread only.
  CURL* easy() override { return easy_.get(); }
  void OnDone(CURLcode result) override;
  // The connect-only handle must stay attached to the multi handle while the
  // WebSocket is in use; removing it would close the connection.
  bool RemoveFromMultiOnDone() override { return false; }

  // Runs fn on the I/O thread and blocks until it has completed (runs inline
  // if already on the I/O thread).
  void RunOnLoopAndWait(std::function<void()> fn);

  // The following run on the I/O thread only.

  // Start the asynchronous WebSocket handshake.
  void ConnectOnLoop(int timeout_ms);
  // Gracefully close the WebSocket connection and notify the handler.
  void CloseOnLoop();
  // Queue and flush an outbound text message.
  void SendOnLoop(const std::string& msg);
  // Destructor path: release every I/O-thread resource without callbacks.
  void TeardownOnLoop();

  // Extra-fd readiness callback while open: flush, receive, re-arm.
  void Pump(short revents);
  // Attempt to flush any queued outbound messages via curl_ws_send().
  void FlushSendQueue();
  // Drain all available incoming WebSocket frames via curl_ws_recv().
  void ReceiveFrames();
  // Re-arm the extra fd for POLLIN plus POLLOUT if data is waiting.
  void UpdateExtraFdEvents();
  // Unregister the socket and handshake transfer, detach and destroy the easy
  // handle and reset all per-connection buffers.
  void CleanupEasyHandle();
  // Check if the WebSocket connection is currently established and open.
  bool IsWebSocketAvailable() const;

  // Event notifications matching the WebSocketClientEventHandler contract;
  // each one marshals to scheduler_ via ScheduleHandlerEvent().
  void ScheduleHandlerEvent(
      std::function<void(WebSocketClientEventHandler*)> fn);
  void OnError();
  void OnConnection();
  void OnMessage(const char* message, size_t length);
  void OnDisconnection();

  // The server URI to connect to ("ws://..." or "wss://...").
  const std::string uri_;

  // The event handler for connection events.
  WebSocketClientEventHandler* handler_;

  // The shared I/O loop. Valid for the lifetime of this object because the
  // constructor acquires (and the destructor releases) a transport reference.
  firebase::rest::curl::CurlIoLoop* loop_;

  // The active libcurl easy handle for the WebSocket connection.
  firebase::rest::curl::UniqueEasy easy_;

  // HTTP Upgrade request headers associated with easy_.
  firebase::rest::curl::UniqueSList request_headers_;

  // The connected socket registered with the loop while open, otherwise
  // CURL_SOCKET_BAD.
  curl_socket_t socket_;

  // Current connection state.
  State state_;

  // Buffer for reassembling multi-chunk or fragmented (CURLWS_CONT) incoming
  // WebSocket data frames.
  std::string incoming_message_buffer_;

  struct OutboundFrame {
    std::string payload;
    unsigned int flags;
  };

  // Buffer for reassembling incoming PING payloads if split across chunks.
  std::string incoming_ping_buffer_;

  // Queue of outbound WebSocket frames (CURLWS_TEXT data frames and
  // CURLWS_PONG control replies) waiting to be sent over the socket.
  std::deque<OutboundFrame> send_queue_;

  // True once curl_ws_send() has been invoked for send_queue_.front() and
  // libcurl may hold encoded header/payload bytes in its internal sendbuf,
  // even when curl_ws_send() returned CURLE_AGAIN with sent == 0.
  bool frame_in_progress_;

  // Byte offset into send_queue_.front().payload for partial non-blocking
  // writes.
  size_t current_send_offset_;

  // User agent used when opening the connection.
  std::string user_agent_;

  // Mutex guarding app_check_token_ (written from the scheduler thread, read
  // on the I/O thread when connecting).
  Mutex app_check_token_mutex_;

  // App Check token used when opening the connection.
  std::string app_check_token_;

  Logger* logger_;

  scheduler::Scheduler* scheduler_;

  // Safe reference to this. Set in constructor and cleared in destructor.
  typedef firebase::internal::SafeReference<WebSocketClientImpl> ClientRef;
  typedef firebase::internal::SafeReferenceLock<WebSocketClientImpl>
      ClientRefLock;
  ClientRef safe_this_;

  friend class PersistentConnectionTest;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_
