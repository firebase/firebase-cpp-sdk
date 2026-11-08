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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include "app/rest/curl/curl_handles.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/logger.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "app/src/thread.h"
#include "curl/curl.h"
#include "database/src/desktop/connection/web_socket_client_interface.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

// libcurl-based WebSocket client.
//
// Threading model
// ---------------
// * Each client owns one I/O thread for the lifetime of the object. That
//   thread drives a private CURLM multi handle through the HTTP Upgrade
//   handshake (CURLOPT_CONNECT_ONLY = 2) and then services the connected
//   socket with curl_ws_recv()/curl_ws_send(), sleeping in curl_multi_poll()
//   between events. Nothing is shared with the REST transport except
//   libcurl's global state (AcquireCurlGlobal()) and the TLS policy.
// * Connect(), Close(), Send() and the destructor may be called from any
//   thread (in practice the Database scheduler thread). They post work to the
//   I/O thread and return immediately; the destructor posts a teardown and
//   joins the thread, so no I/O-thread work touches this object after it
//   returns.
// * WebSocketClientEventHandler callbacks (OnOpen/OnMessage/OnClose/OnError)
//   are never invoked on the I/O thread: they are marshalled to the Database
//   `scheduler_` through `safe_this_`, which also makes them no-ops after the
//   client is destroyed.
class WebSocketClientImpl : public WebSocketClientInterface {
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

  // Queues fn to run on the I/O thread and wakes it. Thread-safe.
  void Post(std::function<void()> fn);

  // Entry point and event loop for the background WebSocket I/O thread.
  static void EventLoopRoutine(void* client);
  void RunEventLoop();
  // Runs every queued function in `posted_`. I/O thread only.
  void RunPosted();

  // The following run on the I/O thread only.

  // Start the asynchronous WebSocket handshake.
  void ConnectOnLoop(int timeout_ms);
  // Handshake finished (CURLMSG_DONE): either fail or switch to kStateOpen.
  void OnHandshakeDone(CURLcode result);
  // Gracefully close the WebSocket connection and notify the handler.
  void CloseOnLoop();
  // Queue and flush an outbound text message.
  void SendOnLoop(const std::string& msg);
  // Destructor path: release every I/O-thread resource without callbacks.
  void TeardownOnLoop();

  // Socket readiness while open (`revents` is a CURL_WAIT_* mask): flush and
  // receive.
  void Pump(int revents);
  // Attempt to flush any queued outbound messages via curl_ws_send().
  void FlushSendQueue();
  // Drain all available incoming WebSocket frames via curl_ws_recv().
  void ReceiveFrames();
  // Detach and destroy the easy handle and reset all per-connection buffers.
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

  // Private multi handle: drives the handshake and provides
  // curl_multi_poll()/curl_multi_wakeup() for the connected socket.
  firebase::rest::curl::UniqueMulti multi_;

  // Mutex protecting `posted_`, the queue of closures posted via Post() from
  // other threads (Connect(), Send(), Close(), and ~WebSocketClientImpl()) to
  // be drained on the background I/O thread.
  Mutex post_mutex_;
  std::deque<std::function<void()>> posted_;

  // Set by the destructor's posted teardown to exit RunEventLoop().
  bool quit_;

  // Set by ReceiveFrames() when it stopped after kMaxFramesPerReceivePass with
  // frames possibly still buffered in libcurl; RunEventLoop() then polls
  // without blocking. I/O thread only.
  bool receive_backlog_;

  // Set by Post() (any thread) once curl_multi_wakeup() has failed; the loop
  // then uses a short poll timeout so posted work still runs promptly.
  std::atomic<bool> wakeup_failed_;

  // The active libcurl easy handle for the WebSocket connection. Attached to
  // multi_ from ConnectOnLoop() until CleanupEasyHandle(): libcurl closes a
  // connect-only connection as soon as its handle is removed.
  firebase::rest::curl::UniqueEasy easy_;

  // HTTP Upgrade request headers associated with easy_.
  firebase::rest::curl::UniqueSList request_headers_;

  // The connected socket while open, otherwise CURL_SOCKET_BAD.
  curl_socket_t socket_;

  // Current connection state.
  State state_;

  // Deadline for the HTTP Upgrade exchange. libcurl bounds only DNS/TCP/TLS
  // for a CURLOPT_CONNECT_ONLY handle, so RunEventLoop() enforces this itself.
  std::chrono::steady_clock::time_point handshake_deadline_;

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

  // Started last in the constructor and joined in the destructor body, so
  // every member it uses outlives it.
  Thread thread_;

  friend class PersistentConnectionTest;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_
