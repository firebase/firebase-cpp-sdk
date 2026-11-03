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

#include "database/src/desktop/connection/web_socket_client_impl.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "app/rest/curl/tls_policy.h"
#include "app/rest/transport_curl.h"
#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/thread.h"
#include "curl/curl.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

namespace {
// Size of the stack buffer used for each curl_ws_recv() read pass.
constexpr size_t kRecvBufferSize = 16384;
// RFC 6455 section 5.5 caps all WebSocket control frame payloads at 125 bytes
// and forbids fragmenting control frames.
constexpr size_t kMaxPingPayloadBytes = 125;
// Upper bound on a single reassembled inbound WebSocket message (256 MiB,
// matching Realtime Database's single-read payload limit) so a misbehaving peer
// streaming CURLWS_CONT fragments cannot grow memory without bound.
constexpr size_t kMaxMessagePayloadBytes = 256 * 1024 * 1024;
// Poll timeout in milliseconds while waiting for handshake completion.
constexpr int kConnectingPollTimeoutMs = 100;
// Poll timeout in milliseconds while connected or idle (woken immediately by
// curl_multi_wakeup() on state changes).
constexpr int kDefaultPollTimeoutMs = 1000;
}  // namespace

WebSocketClientImpl::WebSocketClientImpl(
    const std::string& uri, const std::string& user_agent, Logger* logger,
    scheduler::Scheduler* scheduler, const std::string& app_check_token,
    WebSocketClientEventHandler* handler /*=nullptr*/)
    : uri_(uri),
      handler_(handler),
      thread_(nullptr),
      curl_multi_(nullptr),
      curl_easy_(nullptr),
      request_headers_(nullptr),
      state_(kStateIdle),
      incoming_message_buffer_(),
      incoming_ping_buffer_(),
      send_queue_(),
      current_send_offset_(0),
      callback_queue_(),
      callback_queue_mutex_(Mutex::kModeNonRecursive),
      is_destructing_(0),
      user_agent_(user_agent),
      app_check_token_(app_check_token),
      logger_(logger),
      scheduler_(scheduler),
      safe_this_(this) {
  firebase::rest::InitTransportCurl();
  curl_multi_ = curl_multi_init();
  FIREBASE_DEV_ASSERT(curl_multi_ != nullptr);

  // Start the background event loop thread.
  thread_ = std::make_unique<Thread>(EventLoopRoutine, this);
}

void WebSocketClientImpl::EventLoopRoutine(void* data) {
  assert(data != nullptr);
  WebSocketClientImpl* client = static_cast<WebSocketClientImpl*>(data);
  client->RunEventLoop();
}

void WebSocketClientImpl::RunEventLoop() {
  logger_->LogDebug("=== libcurl WebSocket Event Loop Start ===");
  CURLM* multi = static_cast<CURLM*>(curl_multi_);

  while (is_destructing_.load() == 0) {
    ProcessCallbackQueue();
    if (is_destructing_.load() > 0) {
      break;
    }

    if (state_ == kStateConnecting) {
      int running_handles = 0;
      CURLMcode mc = curl_multi_perform(multi, &running_handles);
      if (mc != CURLM_OK) {
        logger_->LogError("curl_multi_perform failed: %s",
                          curl_multi_strerror(mc));
        CleanupEasyHandle();
        state_ = kStateClosed;
        OnError();
        continue;
      }

      int msgs_left = 0;
      CURLMsg* msg = nullptr;
      while ((msg = curl_multi_info_read(multi, &msgs_left)) != nullptr) {
        if (msg->msg == CURLMSG_DONE && msg->easy_handle == curl_easy_) {
          CURLcode result = msg->data.result;
          if (result == CURLE_OK) {
            // Clear the transfer timeout once the WebSocket upgrade succeeds so
            // long-lived connections are not terminated after timeout_ms.
            curl_easy_setopt(static_cast<CURL*>(curl_easy_), CURLOPT_TIMEOUT_MS,
                             0L);
            state_ = kStateOpen;
            OnConnection();
            // Drain any WebSocket frames that arrived in the same packet or TLS
            // record as the HTTP 101 Switching Protocols response.
            if (state_ == kStateOpen && is_destructing_.load() == 0) {
              ReceiveFrames();
            }
            if (state_ == kStateOpen && is_destructing_.load() == 0) {
              FlushSendQueue();
            }
          } else {
            logger_->LogDebug(
                "Error occurred while establishing connection to "
                "%s: (%d) %s",
                uri_.c_str(), static_cast<int>(result),
                curl_easy_strerror(result));
            CleanupEasyHandle();
            state_ = kStateClosed;
            OnError();
          }
          break;
        }
      }

      if (state_ == kStateConnecting && is_destructing_.load() == 0) {
        curl_multi_poll(multi, nullptr, 0, kConnectingPollTimeoutMs, nullptr);
      }
    } else if (state_ == kStateOpen) {
      FlushSendQueue();
      if (state_ == kStateOpen && is_destructing_.load() == 0) {
        ReceiveFrames();
      }
      if (state_ == kStateOpen && is_destructing_.load() == 0) {
        curl_socket_t sockfd = CURL_SOCKET_BAD;
        curl_easy_getinfo(static_cast<CURL*>(curl_easy_), CURLINFO_ACTIVESOCKET,
                          &sockfd);
        if (sockfd != CURL_SOCKET_BAD) {
          struct curl_waitfd waitfd;
          waitfd.fd = sockfd;
          waitfd.events = static_cast<short>(
              CURL_WAIT_POLLIN | (send_queue_.empty() ? 0 : CURL_WAIT_POLLOUT));
          waitfd.revents = 0;
          curl_multi_poll(multi, &waitfd, 1, kDefaultPollTimeoutMs, nullptr);
        } else {
          curl_multi_poll(multi, nullptr, 0, kConnectingPollTimeoutMs, nullptr);
        }
      }
    } else {
      // Idle or closed: block until woken by curl_multi_wakeup() or timeout.
      curl_multi_poll(multi, nullptr, 0, kDefaultPollTimeoutMs, nullptr);
    }
  }

  // Clean up any active handle during destruction.
  if (curl_easy_ != nullptr) {
    if (state_ == kStateOpen && current_send_offset_ == 0) {
      const unsigned char close_payload[2] = {0x03, 0xe8};  // 1000 Normal
      size_t sent = 0;
      (void)curl_ws_send(static_cast<CURL*>(curl_easy_), close_payload,
                         sizeof(close_payload), &sent, 0, CURLWS_CLOSE);
    }
    CleanupEasyHandle();
    state_ = kStateClosed;
  }

  logger_->LogDebug("=== libcurl WebSocket Event Loop End ===");
}

WebSocketClientImpl::~WebSocketClientImpl() {
  // Clear safe reference immediately so that scheduled callbacks can skip
  // executing code which requires a reference to this.
  safe_this_.ClearReference();

  is_destructing_.store(1);

  if (curl_multi_ != nullptr) {
    curl_multi_wakeup(static_cast<CURLM*>(curl_multi_));
  }

  // Wait for the event loop thread to end.
  if (thread_) {
    thread_->Join();
    thread_.reset(nullptr);
  }

  if (curl_multi_ != nullptr) {
    curl_multi_cleanup(static_cast<CURLM*>(curl_multi_));
    curl_multi_ = nullptr;
  }

  firebase::rest::CleanupTransportCurl();

  handler_ = nullptr;
  assert(curl_easy_ == nullptr);
}

void WebSocketClientImpl::Connect(int timeout_ms) {
  ScheduleOnce([](WebSocketClientImpl* client, int timeout_ms,
                  const std::string&) { client->ConnectSync(timeout_ms); },
               timeout_ms, "");
}

void WebSocketClientImpl::ConnectSync(int timeout_ms) {
  if (state_ == kStateConnecting || state_ == kStateOpen) {
    logger_->LogWarning("websocket has already been connected to %s",
                        uri_.c_str());
    return;
  }

  if (curl_easy_ != nullptr) {
    CleanupEasyHandle();
  }

  CURL* easy = curl_easy_init();
  if (easy == nullptr) {
    logger_->LogError("Failed to initialize curl easy handle for %s",
                      uri_.c_str());
    OnError();
    return;
  }

  curl_easy_setopt(easy, CURLOPT_URL, uri_.c_str());
  // Restrict schemes to ws/wss so system-libcurl builds that compile in extra
  // protocols (file://, http://, ftp://, ...) cannot open non-WebSocket URIs.
  CURLcode proto_result =
      curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "ws,wss");
  if (proto_result != CURLE_OK) {
    logger_->LogError("Failed to restrict WebSocket protocols for %s: (%d) %s",
                      uri_.c_str(), static_cast<int>(proto_result),
                      curl_easy_strerror(proto_result));
    curl_easy_cleanup(easy);
    OnError();
    return;
  }
  // CURLOPT_CONNECT_ONLY = 2L instructs libcurl to perform the HTTP/1.1
  // WebSocket Upgrade handshake and stop after receiving 101 Switching
  // Protocols so framing can be driven via curl_ws_recv() and curl_ws_send().
  curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 2L);
  // Handle PING frames explicitly in ReceiveFrames() so standalone PINGs flush
  // their PONG response immediately without waiting for another frame.
  curl_easy_setopt(easy, CURLOPT_WS_OPTIONS, CURLWS_NOAUTOPONG);
  // Peer/hostname verification, TLS 1.2 minimum, and OS trust store selection
  // are centralized in app/rest/curl/tls_policy.h so REST and WebSocket
  // connections can never drift apart.
  CURLcode tls_result = firebase::rest::curl::ApplyTlsPolicy(easy);
  if (tls_result != CURLE_OK) {
    logger_->LogError("Failed to apply TLS policy for %s: (%d) %s",
                      uri_.c_str(), static_cast<int>(tls_result),
                      curl_easy_strerror(tls_result));
    curl_easy_cleanup(easy);
    OnError();
    return;
  }
  if (timeout_ms > 0) {
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS,
                     static_cast<long>(timeout_ms));  // NOLINT
    curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS,
                     static_cast<long>(timeout_ms));  // NOLINT
  }

  std::string app_check_token;
  {
    MutexLock lock(callback_queue_mutex_);
    app_check_token = app_check_token_;
  }

  std::string user_agent_header = "User-Agent: " + user_agent_;
  request_headers_ =
      curl_slist_append(request_headers_, user_agent_header.c_str());

  const char* api_client = App::GetUserAgent();
  if (api_client != nullptr && api_client[0] != '\0') {
    std::string api_client_header =
        std::string(app_common::kApiClientHeader) + ": " + api_client;
    request_headers_ =
        curl_slist_append(request_headers_, api_client_header.c_str());
  }

  if (!app_check_token.empty()) {
    std::string app_check_header = "X-Firebase-AppCheck: " + app_check_token;
    request_headers_ =
        curl_slist_append(request_headers_, app_check_header.c_str());
  }

  curl_easy_setopt(easy, CURLOPT_HTTPHEADER, request_headers_);

  CURLMcode mc = curl_multi_add_handle(static_cast<CURLM*>(curl_multi_), easy);
  if (mc != CURLM_OK) {
    logger_->LogError("curl_multi_add_handle failed: %s",
                      curl_multi_strerror(mc));
    if (request_headers_ != nullptr) {
      curl_slist_free_all(request_headers_);
      request_headers_ = nullptr;
    }
    curl_easy_cleanup(easy);
    OnError();
    return;
  }

  curl_easy_ = easy;
  incoming_message_buffer_.clear();
  incoming_ping_buffer_.clear();
  send_queue_.clear();
  current_send_offset_ = 0;
  state_ = kStateConnecting;
}

void WebSocketClientImpl::Close() {
  ScheduleOnce([](WebSocketClientImpl* client, int,
                  const std::string&) { client->CloseSync(); },
               0, "");
}

void WebSocketClientImpl::CloseSync() {
  if (state_ == kStateOpen && curl_easy_ != nullptr) {
    if (current_send_offset_ == 0) {
      // Send close code 1000 (Normal Closure) in big-endian byte order.
      const unsigned char close_payload[2] = {0x03, 0xe8};
      size_t sent = 0;
      (void)curl_ws_send(static_cast<CURL*>(curl_easy_), close_payload,
                         sizeof(close_payload), &sent, 0, CURLWS_CLOSE);
    }
    CleanupEasyHandle();
    state_ = kStateClosed;
    OnDisconnection();
  } else if (state_ == kStateConnecting && curl_easy_ != nullptr) {
    CleanupEasyHandle();
    state_ = kStateClosed;
  }
}

void WebSocketClientImpl::Send(const char* msg) {
  assert(msg != nullptr);

  ScheduleOnce([](WebSocketClientImpl* client, int,
                  const std::string& msg) { client->SendSync(msg); },
               0, msg);
}

void WebSocketClientImpl::SendSync(const std::string& msg) {
  if (IsWebSocketAvailable()) {
    send_queue_.push_back(msg);
    FlushSendQueue();
  } else {
    logger_->LogWarning("Cannot send message.  websocket is not available");
  }
}

void WebSocketClientImpl::FlushSendQueue() {
  if (!IsWebSocketAvailable()) {
    return;
  }

  CURL* easy = static_cast<CURL*>(curl_easy_);
  while (!send_queue_.empty() && state_ == kStateOpen &&
         is_destructing_.load() == 0) {
    const std::string& msg = send_queue_.front();
    const char* data = msg.data() + current_send_offset_;
    size_t remaining = msg.size() - current_send_offset_;
    size_t sent = 0;

    CURLcode res = curl_ws_send(easy, data, remaining, &sent, 0, CURLWS_TEXT);
    if (res == CURLE_OK || res == CURLE_AGAIN) {
      current_send_offset_ += sent;
      if (current_send_offset_ >= msg.size() && res == CURLE_OK) {
        send_queue_.pop_front();
        current_send_offset_ = 0;
      } else {
        // Socket write buffer is full; wait for CURL_WAIT_POLLOUT.
        break;
      }
    } else {
      logger_->LogError("curl_ws_send failed for %s: (%d) %s", uri_.c_str(),
                        static_cast<int>(res), curl_easy_strerror(res));
      CleanupEasyHandle();
      state_ = kStateClosed;
      OnError();
      OnDisconnection();
      break;
    }
  }
}

void WebSocketClientImpl::ReceiveFrames() {
  if (!IsWebSocketAvailable()) {
    return;
  }

  CURL* easy = static_cast<CURL*>(curl_easy_);
  char buffer[kRecvBufferSize];

  while (state_ == kStateOpen && is_destructing_.load() == 0) {
    size_t rlen = 0;
    const struct curl_ws_frame* meta = nullptr;
    CURLcode res = curl_ws_recv(easy, buffer, sizeof(buffer), &rlen, &meta);

    if (res == CURLE_AGAIN) {
      break;
    }
    if (res == CURLE_GOT_NOTHING) {
      CleanupEasyHandle();
      state_ = kStateClosed;
      OnDisconnection();
      break;
    }
    if (res != CURLE_OK) {
      logger_->LogDebug("curl_ws_recv error on %s: (%d) %s", uri_.c_str(),
                        static_cast<int>(res), curl_easy_strerror(res));
      CleanupEasyHandle();
      state_ = kStateClosed;
      OnError();
      OnDisconnection();
      break;
    }

    if (meta == nullptr) {
      break;
    }

    if (meta->flags & CURLWS_CLOSE) {
      if (current_send_offset_ == 0) {
        size_t sent = 0;
        (void)curl_ws_send(easy, "", 0, &sent, 0, CURLWS_CLOSE);
      }
      CleanupEasyHandle();
      state_ = kStateClosed;
      OnDisconnection();
      break;
    }

    const uint64_t frame_remaining =
        meta->bytesleft > 0 ? static_cast<uint64_t>(meta->bytesleft) : 0;

    if (meta->flags & CURLWS_PING) {
      if ((meta->flags & CURLWS_CONT) != 0 ||
          rlen > kMaxPingPayloadBytes - incoming_ping_buffer_.size() ||
          frame_remaining >
              kMaxPingPayloadBytes - (incoming_ping_buffer_.size() + rlen)) {
        logger_->LogError(
            "Invalid or oversized WebSocket PING frame from %s; closing",
            uri_.c_str());
        CleanupEasyHandle();
        state_ = kStateClosed;
        OnError();
        OnDisconnection();
        break;
      }
      if (rlen > 0) {
        incoming_ping_buffer_.append(buffer, rlen);
      }
      if (meta->bytesleft == 0) {
        if (current_send_offset_ == 0) {
          size_t sent = 0;
          (void)curl_ws_send(easy, incoming_ping_buffer_.data(),
                             incoming_ping_buffer_.size(), &sent, 0,
                             CURLWS_PONG);
        }
        incoming_ping_buffer_.clear();
      }
      continue;
    }

    if (meta->flags & CURLWS_PONG) {
      continue;
    }

    if (meta->flags & (CURLWS_TEXT | CURLWS_BINARY | CURLWS_CONT)) {
      if (rlen > kMaxMessagePayloadBytes - incoming_message_buffer_.size() ||
          frame_remaining >
              kMaxMessagePayloadBytes -
                  (incoming_message_buffer_.size() + rlen)) {
        logger_->LogError(
            "Inbound WebSocket message from %s exceeds %zu-byte limit; closing",
            uri_.c_str(), kMaxMessagePayloadBytes);
        CleanupEasyHandle();
        state_ = kStateClosed;
        OnError();
        OnDisconnection();
        break;
      }
      if (rlen > 0) {
        incoming_message_buffer_.append(buffer, rlen);
      }
      if (meta->bytesleft == 0 && (meta->flags & CURLWS_CONT) == 0) {
        std::string completed_message;
        completed_message.swap(incoming_message_buffer_);
        OnMessage(completed_message.data(), completed_message.size());
      }
    }
  }
}

void WebSocketClientImpl::CleanupEasyHandle() {
  if (curl_easy_ != nullptr) {
    if (curl_multi_ != nullptr) {
      curl_multi_remove_handle(static_cast<CURLM*>(curl_multi_),
                               static_cast<CURL*>(curl_easy_));
    }
    curl_easy_cleanup(static_cast<CURL*>(curl_easy_));
    curl_easy_ = nullptr;
  }
  if (request_headers_ != nullptr) {
    curl_slist_free_all(request_headers_);
    request_headers_ = nullptr;
  }
  incoming_message_buffer_.clear();
  incoming_ping_buffer_.clear();
  send_queue_.clear();
  current_send_offset_ = 0;
}

void WebSocketClientImpl::RefreshAppCheckToken(const std::string& token) {
  MutexLock lock(callback_queue_mutex_);
  app_check_token_ = token;
}

void WebSocketClientImpl::OnError() {
  if (handler_) {
    WebSocketClientErrorData error(uri_.c_str());
    handler_->OnError(error);
  }

  logger_->LogDebug("Error occurred while establishing connection to %s",
                    uri_.c_str());
}

void WebSocketClientImpl::OnConnection() {
  if (handler_) {
    scheduler_->Schedule(new callback::CallbackValue1<ClientRef>(
        safe_this_, [](ClientRef client_ref) {
          ClientRefLock lock(&client_ref);
          auto client = lock.GetReference();
          if (client != nullptr && client->handler_ != nullptr) {
            client->handler_->OnOpen();
          }
        }));
  }

  if (is_destructing_.load() > 0) {
    CloseSync();
  }
}

void WebSocketClientImpl::OnMessage(const char* message, size_t length) {
  if (handler_) {
    std::string message_string(message, length);
    scheduler_->Schedule(new callback::CallbackValue2<ClientRef, std::string>(
        safe_this_, message_string, [](ClientRef client_ref, std::string msg) {
          ClientRefLock lock(&client_ref);
          auto client = lock.GetReference();
          if (client != nullptr && client->handler_ != nullptr) {
            client->handler_->OnMessage(msg.c_str());
          }
        }));
  }
}

void WebSocketClientImpl::OnDisconnection() {
  if (handler_) {
    scheduler_->Schedule(new callback::CallbackValue1<ClientRef>(
        safe_this_, [](ClientRef client_ref) {
          ClientRefLock lock(&client_ref);
          auto client = lock.GetReference();
          if (client != nullptr && client->handler_ != nullptr) {
            client->handler_->OnClose();
          }
        }));
  }
}

void WebSocketClientImpl::ScheduleOnce(Callback cb, int int_value,
                                       const std::string& string_value) {
  assert(cb != nullptr);

  {
    MutexLock lock(callback_queue_mutex_);
    callback_queue_.push(CallbackData(cb, this, int_value, string_value));
  }

  if (curl_multi_ != nullptr) {
    curl_multi_wakeup(static_cast<CURLM*>(curl_multi_));
  }
}

void WebSocketClientImpl::ProcessCallbackQueue() {
  for (;;) {
    CallbackData callback_data(nullptr, nullptr, 0, "");
    {
      MutexLock lock(callback_queue_mutex_);
      if (callback_queue_.empty()) {
        break;
      }
      callback_data = callback_queue_.front();
      callback_queue_.pop();
    }
    callback_data.callback(callback_data.client, callback_data.int_value,
                           callback_data.string_value);
  }
}

bool WebSocketClientImpl::IsWebSocketAvailable() const {
  return state_ == kStateOpen && curl_easy_ != nullptr &&
         is_destructing_.load() == 0;
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
