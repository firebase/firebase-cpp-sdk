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
#include <cstddef>
#include <deque>
#include <memory>
#include <queue>
#include <string>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/logger.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "app/src/thread.h"
#include "database/src/desktop/connection/web_socket_client_interface.h"

struct curl_slist;

namespace firebase {
namespace database {
namespace internal {
namespace connection {

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
  typedef void (*Callback)(WebSocketClientImpl* client, int int_value,
                           const std::string& string_value);

  enum State {
    kStateIdle = 0,
    kStateConnecting,
    kStateOpen,
    kStateClosed,
  };

  // The thread routine to host the libcurl WebSocket event loop.
  static void EventLoopRoutine(void* data);

  // Main event loop executed on thread_.
  void RunEventLoop();

  // Start the asynchronous WebSocket handshake on the event loop thread.
  void ConnectSync(int timeout_ms);

  // Synchronously close the WebSocket connection on the event loop thread.
  void CloseSync();

  // Queue and flush an outbound text message on the event loop thread.
  void SendSync(const std::string& msg);

  // Attempt to flush any queued outbound messages via curl_ws_send().
  void FlushSendQueue();

  // Drain all available incoming WebSocket frames via curl_ws_recv().
  void ReceiveFrames();

  // Remove and clean up the active easy handle and request headers.
  void CleanupEasyHandle();

  // Event notifications matching the WebSocketClientEventHandler contract.
  void OnError();
  void OnConnection();
  void OnMessage(const char* message, size_t length);
  void OnDisconnection();

  // Schedule an async callback to be triggered in the next iteration of the
  // event loop. This call is thread-safe.
  void ScheduleOnce(Callback cb, int int_value,
                    const std::string& string_value);

  // Process all pending callbacks in callback_queue_ on the event loop thread.
  void ProcessCallbackQueue();

  // Check if the WebSocket connection is currently established and open.
  // Only call this in the event loop thread.
  bool IsWebSocketAvailable() const;

  // The server URI to connect to ("ws://..." or "wss://...").
  const std::string uri_;

  // The event handler for connection events.
  WebSocketClientEventHandler* handler_;

  // The worker thread hosting the libcurl multi event loop.
  std::unique_ptr<Thread> thread_;

  // The libcurl multi handle (CURLM*) used to drive non-blocking handshakes,
  // socket polling (curl_multi_poll), and cross-thread wakeups
  // (curl_multi_wakeup).
  void* curl_multi_;

  // The active libcurl easy handle (CURL*) for the WebSocket connection.
  // Only accessed on the event loop thread.
  void* curl_easy_;

  // Linked list of HTTP Upgrade request headers associated with curl_easy_.
  struct curl_slist* request_headers_;

  // Current connection state on the event loop thread.
  State state_;

  // Buffer for reassembling multi-chunk or fragmented (CURLWS_CONT) incoming
  // WebSocket data frames.
  std::string incoming_message_buffer_;

  // Buffer for reassembling incoming PING payloads if split across chunks.
  std::string incoming_ping_buffer_;

  // Queue of outbound text messages waiting to be sent over the WebSocket.
  std::deque<std::string> send_queue_;

  // Byte offset into send_queue_.front() for partial non-blocking writes.
  size_t current_send_offset_;

  // A blob of callback data added to callback_queue_ and triggered on the
  // event loop thread.
  struct CallbackData {
    explicit CallbackData(Callback c, WebSocketClientImpl* ws_client, int i,
                          const std::string& str)
        : callback(c), client(ws_client), int_value(i), string_value(str) {}

    Callback callback;
    WebSocketClientImpl* client;
    int int_value;
    std::string string_value;
  };

  // A queue of callbacks to be triggered on the event loop thread.
  std::queue<CallbackData> callback_queue_;

  // Mutex guarding callback_queue_ and app_check_token_.
  Mutex callback_queue_mutex_;

  // Flagged when this object starts to be destructed so the event loop thread
  // can exit immediately.
  std::atomic<int> is_destructing_;

  // User agent used when opening the connection.
  std::string user_agent_;

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
