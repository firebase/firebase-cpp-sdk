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
#include "app/rest/curl/transfer.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/logger.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "curl/curl.h"
#include "database/src/desktop/connection/web_socket_client_interface.h"

namespace firebase {
namespace rest {
namespace curl {
class CurlIoLoop;
}  // namespace curl
}  // namespace rest

namespace database {
namespace internal {
namespace connection {

class WebSocketClientImpl : public WebSocketClientInterface,
                            private firebase::rest::curl::Transfer {
 public:
  WebSocketClientImpl(const std::string& uri, const std::string& user_agent,
                      Logger* logger, scheduler::Scheduler* scheduler,
                      const std::string& app_check_token,
                      WebSocketClientEventHandler* handler = nullptr);
  ~WebSocketClientImpl() override;

  WebSocketClientImpl(const WebSocketClientImpl&) = delete;
  WebSocketClientImpl& operator=(const WebSocketClientImpl&) = delete;

  void Connect(int timeout_ms) override;
  void Close() override;
  void Send(const char* msg) override;
  void RefreshAppCheckToken(const std::string& token);

 private:
  enum State {
    kStateIdle = 0,
    kStateConnecting,
    kStateOpen,
    kStateClosed,
  };

  CURL* easy() override { return easy_.get(); }
  void OnDone(CURLcode result) override;
  bool RemoveFromMultiOnDone() override { return false; }

  bool PostToLoop(std::function<void()> fn);
  void RunOnLoopAndWait(std::function<void()> fn);

  void ConnectOnLoop(int timeout_ms);
  void CloseOnLoop();
  void SendOnLoop(const std::string& msg);
  void TeardownOnLoop();

  void Pump(short revents);
  void FlushSendQueue();
  void ReceiveFrames();
  void UpdateExtraFdEvents();
  void CleanupEasyHandle();
  bool IsWebSocketAvailable() const;

  void OnError();
  void OnConnection();
  void OnMessage(const char* message, size_t length);
  void OnDisconnection();

  struct OutboundFrame {
    std::string payload;
    unsigned int flags;
  };

  const std::string uri_;
  WebSocketClientEventHandler* handler_;
  firebase::rest::curl::CurlIoLoop* loop_;
  std::shared_ptr<bool> alive_;
  firebase::rest::curl::UniqueEasy easy_;
  firebase::rest::curl::UniqueSList request_headers_;
  curl_socket_t socket_;
  State state_;
  std::string incoming_message_buffer_;
  std::string incoming_ping_buffer_;
  std::deque<OutboundFrame> send_queue_;
  bool frame_in_progress_;
  size_t current_send_offset_;
  std::string user_agent_;
  Mutex app_check_token_mutex_;
  std::string app_check_token_;
  Logger* logger_;
  scheduler::Scheduler* scheduler_;

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
