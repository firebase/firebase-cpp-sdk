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

#include "database/src/desktop/connection/web_socket_client_impl.h"

#include <cassert>
#include <cstring>
#include <string>
#include <utility>

#include "app/rest/curl/curl_handles.h"
#include "app/rest/curl/curl_io_loop.h"
#include "app/rest/curl/tls_policy.h"
#include "app/rest/transport_curl.h"
#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/semaphore.h"
#include "curl/curl.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

using ::firebase::rest::curl::CurlIoLoop;
using ::firebase::rest::curl::SetOpt;
using ::firebase::rest::curl::SListAppend;
using ::firebase::rest::curl::UniqueEasy;

namespace {
// Size of the stack buffer used for each curl_ws_recv() read pass.
constexpr size_t kRecvBufferSize = 16384;
// Maximum reassembled incoming WebSocket message size (64 MiB).
constexpr size_t kMaxIncomingMessageBytes = 64 * 1024 * 1024;
// Maximum WebSocket PING control frame payload size per RFC 6455 section 5.5.
constexpr size_t kMaxPingPayloadBytes = 125;
// WebSocket close code 1000 (Normal Closure) in network byte order.
const unsigned char kCloseNormalPayload[2] = {0x03, 0xe8};
}  // namespace

WebSocketClientImpl::WebSocketClientImpl(
    const std::string& uri, const std::string& user_agent, Logger* logger,
    scheduler::Scheduler* scheduler, const std::string& app_check_token,
    WebSocketClientEventHandler* handler /*=nullptr*/)
    : uri_(uri),
      handler_(handler),
      loop_(nullptr),
      alive_(std::make_shared<bool>(true)),
      easy_(nullptr),
      request_headers_(nullptr),
      socket_(CURL_SOCKET_BAD),
      state_(kStateIdle),
      incoming_message_buffer_(),
      incoming_ping_buffer_(),
      send_queue_(),
      frame_in_progress_(false),
      current_send_offset_(0),
      user_agent_(user_agent),
      app_check_token_mutex_(Mutex::kModeNonRecursive),
      app_check_token_(app_check_token),
      logger_(logger),
      scheduler_(scheduler),
      safe_this_(this) {
  // Keeps the shared I/O loop (and libcurl's global state) alive for as long
  // as this client exists.
  firebase::rest::InitTransportCurl();
  loop_ = &CurlIoLoop::Get();
}

WebSocketClientImpl::~WebSocketClientImpl() {
  // Clear safe reference immediately so that scheduled callbacks can skip
  // executing code which requires a reference to this.
  safe_this_.ClearReference();

  // Release the socket, the handshake registration and the easy handle on the
  // I/O thread, and wait for that to be done: nothing may touch this object
  // from the I/O thread once we return.
  RunOnLoopAndWait([this]() { TeardownOnLoop(); });

  handler_ = nullptr;
  assert(easy_ == nullptr);
  assert(socket_ == CURL_SOCKET_BAD);

  firebase::rest::CleanupTransportCurl();
}

// ---------------------------------------------------------------------------
// Cross-thread entry points.
// ---------------------------------------------------------------------------

bool WebSocketClientImpl::PostToLoop(std::function<void()> fn) {
  std::shared_ptr<bool> alive = alive_;
  return loop_->Post([alive, fn]() {
    if (*alive) fn();
  });
}

void WebSocketClientImpl::RunOnLoopAndWait(std::function<void()> fn) {
  if (loop_->IsOnIoThread()) {
    fn();
    return;
  }
  Semaphore done(0);
  bool posted = loop_->Post([&fn, &done]() {
    fn();
    done.Post();
  });
  // The loop cannot have stopped while this client holds a transport
  // reference; if it somehow did there is no I/O thread left to clean up on.
  FIREBASE_ASSERT_MESSAGE(posted,
                          "WebSocketClientImpl: shared curl I/O loop stopped "
                          "while a client was still alive");
  if (posted) done.Wait();
}

void WebSocketClientImpl::Connect(int timeout_ms) {
  PostToLoop([this, timeout_ms]() { ConnectOnLoop(timeout_ms); });
}

void WebSocketClientImpl::Close() {
  PostToLoop([this]() { CloseOnLoop(); });
}

void WebSocketClientImpl::Send(const char* msg) {
  assert(msg != nullptr);
  std::string message(msg);
  PostToLoop([this, message]() { SendOnLoop(message); });
}

void WebSocketClientImpl::RefreshAppCheckToken(const std::string& token) {
  MutexLock lock(app_check_token_mutex_);
  app_check_token_ = token;
}

// ---------------------------------------------------------------------------
// Handshake (rest::curl::Transfer).
// ---------------------------------------------------------------------------

void WebSocketClientImpl::ConnectOnLoop(int timeout_ms) {
  if (state_ == kStateConnecting || state_ == kStateOpen) {
    logger_->LogWarning("websocket has already been connected to %s",
                        uri_.c_str());
    return;
  }

  CleanupEasyHandle();

  UniqueEasy easy(curl_easy_init());
  if (easy == nullptr) {
    logger_->LogError("Failed to initialize curl easy handle for %s",
                      uri_.c_str());
    OnError();
    return;
  }

  SetOpt(easy.get(), CURLOPT_URL, uri_.c_str());
  SetOpt(easy.get(), CURLOPT_PROTOCOLS_STR, "ws,wss");
  // CURLOPT_CONNECT_ONLY = 2L instructs libcurl to perform the HTTP/1.1
  // WebSocket Upgrade handshake and stop after receiving 101 Switching
  // Protocols so framing can be driven via curl_ws_recv() and curl_ws_send().
  SetOpt(easy.get(), CURLOPT_CONNECT_ONLY, 2L);
  // Handle PING frames explicitly in ReceiveFrames() so standalone PINGs flush
  // their PONG response immediately without waiting for another frame.
  SetOpt(easy.get(), CURLOPT_WS_OPTIONS,
         static_cast<long>(CURLWS_NOAUTOPONG));  // NOLINT
  // Peer/hostname verification, TLS 1.2 minimum, and OS trust store selection
  // are centralized in app/rest/curl/tls_policy.h so REST and WebSocket
  // connections can never drift apart.
  CURLcode tls_result = firebase::rest::curl::ApplyTlsPolicy(easy.get());
  if (tls_result != CURLE_OK) {
    logger_->LogError("Failed to apply TLS policy for %s: (%d) %s",
                      uri_.c_str(), static_cast<int>(tls_result),
                      curl_easy_strerror(tls_result));
    OnError();
    return;
  }
  if (timeout_ms > 0) {
    SetOpt(easy.get(), CURLOPT_CONNECTTIMEOUT_MS,
           static_cast<long>(timeout_ms));  // NOLINT
    SetOpt(easy.get(), CURLOPT_TIMEOUT_MS,
           static_cast<long>(timeout_ms));  // NOLINT
  }

  std::string app_check_token;
  {
    MutexLock lock(app_check_token_mutex_);
    app_check_token = app_check_token_;
  }

  std::string user_agent_header = "User-Agent: " + user_agent_;
  SListAppend(&request_headers_, user_agent_header.c_str());

  const char* api_client = App::GetUserAgent();
  if (api_client != nullptr && api_client[0] != '\0') {
    std::string api_client_header =
        std::string(app_common::kApiClientHeader) + ": " + api_client;
    SListAppend(&request_headers_, api_client_header.c_str());
  }

  if (!app_check_token.empty()) {
    std::string app_check_header = "X-Firebase-AppCheck: " + app_check_token;
    SListAppend(&request_headers_, app_check_header.c_str());
  }

  SetOpt(easy.get(), CURLOPT_HTTPHEADER, request_headers_.get());

  easy_ = std::move(easy);
  incoming_message_buffer_.clear();
  incoming_ping_buffer_.clear();
  send_queue_.clear();
  frame_in_progress_ = false;
  current_send_offset_ = 0;

  CURLMcode mc = loop_->Add(*this);
  if (mc != CURLM_OK) {
    logger_->LogError("curl_multi_add_handle failed: %s",
                      curl_multi_strerror(mc));
    CleanupEasyHandle();
    OnError();
    return;
  }
  state_ = kStateConnecting;
}

void WebSocketClientImpl::OnDone(CURLcode result) {
  // The loop has forgotten this transfer; the handle is still attached to the
  // multi handle (RemoveFromMultiOnDone() == false).
  if (result != CURLE_OK) {
    logger_->LogDebug(
        "Error occurred while establishing connection to %s: (%d) %s",
        uri_.c_str(), static_cast<int>(result), curl_easy_strerror(result));
    CleanupEasyHandle();
    state_ = kStateClosed;
    OnError();
    return;
  }

  // Clear the transfer timeout once the WebSocket upgrade succeeds so
  // long-lived connections are not terminated after timeout_ms.
  SetOpt(easy_.get(), CURLOPT_TIMEOUT_MS, 0L);

  curl_socket_t sockfd = CURL_SOCKET_BAD;
  curl_easy_getinfo(easy_.get(), CURLINFO_ACTIVESOCKET, &sockfd);
  if (sockfd == CURL_SOCKET_BAD) {
    logger_->LogError("No active socket after WebSocket upgrade to %s",
                      uri_.c_str());
    CleanupEasyHandle();
    state_ = kStateClosed;
    OnError();
    return;
  }

  socket_ = sockfd;
  state_ = kStateOpen;
  loop_->AddExtraFd(socket_, CURL_WAIT_POLLIN,
                    [this](short revents) { Pump(revents); });
  OnConnection();
  // Drain any WebSocket frames that arrived in the same packet or TLS record
  // as the HTTP 101 Switching Protocols response, and flush anything queued.
  Pump(CURL_WAIT_POLLIN | CURL_WAIT_POLLOUT);
}

// ---------------------------------------------------------------------------
// I/O-thread operations.
// ---------------------------------------------------------------------------

void WebSocketClientImpl::CloseOnLoop() {
  if (state_ == kStateOpen && easy_ != nullptr) {
    if (!frame_in_progress_) {
      size_t sent = 0;
      (void)curl_ws_send(easy_.get(), kCloseNormalPayload,
                         sizeof(kCloseNormalPayload), &sent, 0, CURLWS_CLOSE);
    }
    CleanupEasyHandle();
    state_ = kStateClosed;
    OnDisconnection();
  } else if (state_ == kStateConnecting && easy_ != nullptr) {
    CleanupEasyHandle();
    state_ = kStateClosed;
  }
}

void WebSocketClientImpl::SendOnLoop(const std::string& msg) {
  if (IsWebSocketAvailable()) {
    send_queue_.push_back(OutboundFrame{msg, CURLWS_TEXT});
    FlushSendQueue();
    if (state_ == kStateOpen) UpdateExtraFdEvents();
  } else {
    logger_->LogWarning("Cannot send message.  websocket is not available");
  }
}

void WebSocketClientImpl::TeardownOnLoop() {
  *alive_ = false;
  if (state_ == kStateOpen && easy_ != nullptr && !frame_in_progress_) {
    size_t sent = 0;
    (void)curl_ws_send(easy_.get(), kCloseNormalPayload,
                       sizeof(kCloseNormalPayload), &sent, 0, CURLWS_CLOSE);
  }
  CleanupEasyHandle();
  state_ = kStateClosed;
}

void WebSocketClientImpl::Pump(short revents) {
  (void)revents;  // Level-triggered: always try both directions.
  if (state_ != kStateOpen) return;
  FlushSendQueue();
  if (state_ == kStateOpen) ReceiveFrames();
  if (state_ == kStateOpen) UpdateExtraFdEvents();
}

void WebSocketClientImpl::UpdateExtraFdEvents() {
  if (socket_ == CURL_SOCKET_BAD) return;
  short events = CURL_WAIT_POLLIN;
  if (!send_queue_.empty()) events |= CURL_WAIT_POLLOUT;
  loop_->SetExtraFdEvents(socket_, events);
}

void WebSocketClientImpl::FlushSendQueue() {
  if (!IsWebSocketAvailable()) {
    return;
  }

  CURL* easy = easy_.get();
  while (!send_queue_.empty() && state_ == kStateOpen) {
    const OutboundFrame& frame = send_queue_.front();
    const char* data = frame.payload.data() + current_send_offset_;
    size_t remaining = frame.payload.size() - current_send_offset_;
    size_t sent = 0;

    frame_in_progress_ = true;
    CURLcode res = curl_ws_send(easy, data, remaining, &sent, 0, frame.flags);
    if (res == CURLE_OK || res == CURLE_AGAIN) {
      current_send_offset_ += sent;
      if (current_send_offset_ >= frame.payload.size() && res == CURLE_OK) {
        send_queue_.pop_front();
        current_send_offset_ = 0;
        frame_in_progress_ = false;
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

  CURL* easy = easy_.get();
  char buffer[kRecvBufferSize];

  while (state_ == kStateOpen) {
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
      if (!frame_in_progress_) {
        size_t sent = 0;
        (void)curl_ws_send(easy, "", 0, &sent, 0, CURLWS_CLOSE);
      }
      CleanupEasyHandle();
      state_ = kStateClosed;
      OnDisconnection();
      break;
    }

    if (meta->flags & CURLWS_PING) {
      if (rlen > kMaxPingPayloadBytes - incoming_ping_buffer_.size() ||
          meta->bytesleft < 0 ||
          static_cast<size_t>(meta->bytesleft) >
              kMaxPingPayloadBytes - incoming_ping_buffer_.size() - rlen) {
        logger_->LogError(
            "WebSocket PING payload exceeds RFC 6455 125-byte limit on %s",
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
        OutboundFrame pong{std::move(incoming_ping_buffer_), CURLWS_PONG};
        incoming_ping_buffer_.clear();
        if (frame_in_progress_ && !send_queue_.empty()) {
          // A frame is already partially buffered/sent in libcurl's sendbuf;
          // queue the PONG immediately behind it (coalescing if a PONG is
          // already waiting, per RFC 6455 section 5.5.3) so we never splice a
          // control frame mid-frame.
          if (send_queue_.size() > 1 && send_queue_[1].flags == CURLWS_PONG) {
            send_queue_[1].payload = std::move(pong.payload);
          } else {
            send_queue_.insert(send_queue_.begin() + 1, std::move(pong));
          }
        } else {
          send_queue_.push_front(std::move(pong));
          FlushSendQueue();
        }
      }
      continue;
    }

    if (meta->flags & CURLWS_PONG) {
      continue;
    }

    if (meta->flags & (CURLWS_TEXT | CURLWS_BINARY | CURLWS_CONT)) {
      if (rlen > kMaxIncomingMessageBytes - incoming_message_buffer_.size() ||
          meta->bytesleft < 0 ||
          static_cast<size_t>(meta->bytesleft) >
              kMaxIncomingMessageBytes - incoming_message_buffer_.size() -
                  rlen) {
        logger_->LogError(
            "WebSocket incoming message exceeds maximum size (%zu bytes) on %s",
            kMaxIncomingMessageBytes, uri_.c_str());
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
  if (socket_ != CURL_SOCKET_BAD) {
    loop_->RemoveExtraFd(socket_);
    socket_ = CURL_SOCKET_BAD;
  }
  if (easy_ != nullptr) {
    // Still registered for the handshake? Remove() detaches it. Otherwise (the
    // handshake completed and the handle stayed attached) detach it here
    // before destroying it.
    if (!loop_->Remove(*this)) {
      curl_multi_remove_handle(loop_->multi(), easy_.get());
    }
    easy_.reset();
  }
  request_headers_.reset();
  incoming_message_buffer_.clear();
  incoming_ping_buffer_.clear();
  send_queue_.clear();
  frame_in_progress_ = false;
  current_send_offset_ = 0;
}

bool WebSocketClientImpl::IsWebSocketAvailable() const {
  return state_ == kStateOpen && easy_ != nullptr;
}

// ---------------------------------------------------------------------------
// Handler notifications, marshalled to the Database scheduler.
// ---------------------------------------------------------------------------

void WebSocketClientImpl::OnError() {
  logger_->LogDebug("Error occurred while establishing connection to %s",
                    uri_.c_str());
  if (handler_) {
    scheduler_->Schedule(new callback::CallbackValue2<ClientRef, std::string>(
        safe_this_, uri_, [](ClientRef client_ref, std::string uri) {
          ClientRefLock lock(&client_ref);
          auto client = lock.GetReference();
          if (client != nullptr && client->handler_ != nullptr) {
            WebSocketClientErrorData error(uri.c_str());
            client->handler_->OnError(error);
          }
        }));
  }
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

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
