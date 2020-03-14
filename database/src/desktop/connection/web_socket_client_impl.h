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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_

#include <queue>
#include <string>

#include "app/memory/atomic.h"
#include "app/memory/unique_ptr.h"
#include "app/src/logger.h"
#include "app/src/mutex.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "app/src/thread.h"
#include "database/src/desktop/connection/web_socket_client_interface.h"
#include "uWebSockets/src/uWS.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

class WebSocketClientImpl : public WebSocketClientInterface {
 public:
  WebSocketClientImpl(const std::string& uri, const std::string& user_agent,
                      Logger* logger, scheduler::Scheduler* scheduler,
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

 private:
  typedef uWS::WebSocket<uWS::CLIENT> ClientWebSocket;
  typedef void (*Callback)(WebSocketClientImpl* client, int int_value,
                           const std::string& string_value);

  // Callback for hub_ when connection error occurs
  static void OnError(void* data);

  // Callback for hub_ when connection is established
  static void OnConnection(ClientWebSocket* ws, uWS::HttpRequest req);

  // Callback for hub_ when a message is received from the server
  static void OnMessage(ClientWebSocket* ws, char* message, size_t length,
                        uWS::OpCode opCode);

  // Callback for hub_ when the connection is closed
  static void OnDisconnection(ClientWebSocket* ws, int code, char* message,
                              size_t length);

  // The thread routine to host the event loop of hub_
  static void EventLoopRoutine(void* data);

  // Synchronously request to close the websocket.  Should only be called in
  // evnet loop thread.
  void CloseSync();

  // Schedule an async callback to be trigger in the next iteration of the event
  // loop.  This call is thread-safe and is to prevent multiple threads fighting
  // for the same resource, such as websocket_
  void ScheduleOnce(Callback cb, int int_value,
                    const std::string& string_value);

  // Process callback queue in event loop thread
  static void ProcessCallbackQueue(uS::Async* async);

  // Check if the websocket is available and not closed.
  // Only call this in event loop.
  bool IsWebSocketAvailable() const;

  // The server uri to connect to. Should start with "ws://" for non-SSL
  // connection or "wss://" for SSL connection.
  const std::string uri_;

  // The event handler for connection events.
  WebSocketClientEventHandler* handler_;

  // The thread to host the event loop of hub_
  UniquePtr<Thread> thread_;

  // The access point for uWebSockets which contains event loops and
  // different sockets.
  uWS::Hub hub_;

  // The handler to keep the event loop of hub_ alive even there is no
  // connection at all.  Otherwise the loop would stop when there is nothing to
  // handle anymore.  Also used to asynchronously close all async handles and
  // WebSocket connection in destructor.
  uS::Async* keep_loop_alive_;

  // Async handler to process callback queue in event look thread.
  uS::Async* process_queue_async_;

  // A blob of callback data which is added to callback queue and triggered in
  // the event loop thread.  The callback member is called with a reference to
  // the client, int_value and string_value stored in this data structure.
  struct CallbackData {
    explicit CallbackData(Callback c, WebSocketClientImpl* ws_client, int i,
                          const std::string& str)
        : callback(c), client(ws_client), int_value(i), string_value(str) {}

    Callback callback;

    WebSocketClientImpl* client;

    // Generic purposed int value.  Used as timeout in this case.
    int int_value;

    // Generic purposed string value.  Used as message to send in this case.
    std::string string_value;
  };

  // A queue of callback to be triggered in event loop thread.
  std::queue<CallbackData> callback_queue_;

  // Mutex to guard callback_queue_
  Mutex callback_queue_mutex_;

  // Flagged when this object starts to be destructed.  This helps the other
  // thread to handle situation accordingly, ex. if the connection is
  // established after this object starts to be deleted.  Note that this flag is
  // is not guarded by any synchronization primitives and is changed outside of
  // the event loop.  If referenced during the event loop, make sure the racing
  // condition will not cause the problem.
  compat::Atomic<int> is_destructing_;

  // The websocket handler to send message, to receive message and to close the
  // connection.  Should only be used in the event loop.  Not thread safe
  ClientWebSocket* websocket_;

  // User agent used when opening the connection.
  std::string user_agent_;

  Logger* logger_;

  scheduler::Scheduler* scheduler_;

  // Safe reference to this.  Set in constructor and cleared in destructor
  // Should be safe to be copied in any thread because the SharedPtr never
  // changes, until safe_this_ is completely destroyed.
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

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_WEB_SOCKET_CLIENT_IMPL_H_
