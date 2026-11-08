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

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include "app/rest/curl/curl_handles.h"
#include "app/rest/curl/tls_policy.h"
#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "curl/curl.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

using ::firebase::rest::curl::AcquireCurlGlobal;
using ::firebase::rest::curl::ApplyOpt;
using ::firebase::rest::curl::ReleaseCurlGlobal;
using ::firebase::rest::curl::UniqueEasy;

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
// WebSocket close status code 1000 (Normal Closure, RFC 6455 section 7.4.1)
// encoded in network byte order (big-endian) for CURLWS_CLOSE frames.
// libcurl's <curl/websockets.h> defines frame flag bits (CURLWS_CLOSE) but
// does not define RFC 6455 status code constants.
constexpr uint16_t kCloseStatusNormal = 1000;
const unsigned char kCloseNormalPayload[2] = {
    static_cast<unsigned char>((kCloseStatusNormal >> 8) & 0xFF),
    static_cast<unsigned char>(kCloseStatusNormal & 0xFF),
};
// Upper bound on how long the I/O thread sleeps in curl_multi_poll() when
// nothing happens. Post() wakes it immediately via curl_multi_wakeup() and
// curl_multi_poll() already honors libcurl's own timers, so this is only a
// fallback for a failed wakeup; keep it long to avoid idle wakeups.
constexpr int kPollTimeoutMs = 60 * 1000;
// Poll timeout used once curl_multi_wakeup() has been seen to fail, so Post()
// latency (Close, Send, destructor) stays bounded without a working wakeup.
constexpr int kPollTimeoutNoWakeupMs = 50;
// Sleep after a curl_multi_poll() failure so a persistent error cannot turn
// the I/O thread into a busy loop.
constexpr int kPollFailureBackoffMs = 100;
// Maximum number of frames ReceiveFrames() consumes before returning to the
// loop so Post()ed work (Close, Send, destructor teardown) is not starved by
// a peer that streams frames continuously.
constexpr int kMaxFramesPerReceivePass = 256;
}  // namespace

WebSocketClientImpl::WebSocketClientImpl(
    const std::string& uri, const std::string& user_agent, Logger* logger,
    scheduler::Scheduler* scheduler, const std::string& app_check_token,
    WebSocketClientEventHandler* handler /*=nullptr*/)
    : uri_(uri),
      handler_(handler),
      multi_(nullptr),
      post_mutex_(Mutex::kModeNonRecursive),
      posted_(),
      quit_(false),
      receive_backlog_(false),
      wakeup_failed_(false),
      easy_(nullptr),
      request_headers_(nullptr),
      socket_(CURL_SOCKET_BAD),
      state_(kStateIdle),
      handshake_deadline_(std::chrono::steady_clock::time_point::max()),
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
      safe_this_(this),
      thread_() {
  AcquireCurlGlobal();
  multi_.reset(curl_multi_init());
  if (multi_ == nullptr) {
    // Typically fd/memory exhaustion. Leave the client inert: Connect() then
    // reports OnError() instead of the process aborting, and the destructor
    // has no thread to join.
    logger_->LogError("curl_multi_init() failed for %s", uri_.c_str());
    return;
  }
  // curl_multi_cleanup() would otherwise join libcurl's resolver thread pool,
  // which can block the destroying thread (the Database scheduler) for a full
  // DNS timeout if a resolve is in flight when the connection is torn down.
  curl_multi_setopt(multi_.get(), CURLMOPT_QUICK_EXIT, 1L);
  // Started last: everything RunEventLoop() touches is initialized above.
  thread_ = Thread(EventLoopRoutine, this);
}

WebSocketClientImpl::~WebSocketClientImpl() {
  // Clear safe reference immediately so that scheduled callbacks can skip
  // executing code which requires a reference to this.
  safe_this_.ClearReference();

  if (multi_ != nullptr) {
    // Release the socket and the easy handle on the I/O thread, then stop it.
    // Nothing touches this object from the I/O thread once Join() returns.
    Post([this]() {
      TeardownOnLoop();
      quit_ = true;
    });
    thread_.Join();
  }

  handler_ = nullptr;
  assert(easy_ == nullptr);
  assert(socket_ == CURL_SOCKET_BAD);

  multi_.reset();
  ReleaseCurlGlobal();
}

// ---------------------------------------------------------------------------
// Cross-thread entry points.
// ---------------------------------------------------------------------------

void WebSocketClientImpl::Post(std::function<void()> fn) {
  MutexLock lock(post_mutex_);
  posted_.push_back(std::move(fn));
  // curl_multi_wakeup() is thread-safe; multi_ outlives the I/O thread. If it
  // fails the callback still runs, but only after the loop's poll timeout, so
  // RunEventLoop() switches to a short timeout once a failure has been seen.
  CURLMcode mc = curl_multi_wakeup(multi_.get());
  if (mc != CURLM_OK) {
    wakeup_failed_ = true;
    logger_->LogWarning("curl_multi_wakeup failed for %s: %s", uri_.c_str(),
                        curl_multi_strerror(mc));
  }
}

void WebSocketClientImpl::Connect(int timeout_ms) {
  if (multi_ == nullptr) {
    // Constructor could not create the multi handle; there is no I/O thread.
    OnError();
    return;
  }
  Post([this, timeout_ms]() { ConnectOnLoop(timeout_ms); });
}

void WebSocketClientImpl::Close() {
  if (multi_ == nullptr) return;
  Post([this]() { CloseOnLoop(); });
}

void WebSocketClientImpl::Send(const char* msg) {
  assert(msg != nullptr);
  if (multi_ == nullptr) {
    logger_->LogWarning("Cannot send message.  websocket is not available");
    return;
  }
  std::string message(msg);
  Post([this, message = std::move(message)]() { SendOnLoop(message); });
}

void WebSocketClientImpl::RefreshAppCheckToken(const std::string& token) {
  MutexLock lock(app_check_token_mutex_);
  app_check_token_ = token;
}

// ---------------------------------------------------------------------------
// The I/O thread.
// ---------------------------------------------------------------------------

void WebSocketClientImpl::EventLoopRoutine(void* client) {
  static_cast<WebSocketClientImpl*>(client)->RunEventLoop();
}

void WebSocketClientImpl::RunPosted() {
  std::deque<std::function<void()>> batch;
  {
    MutexLock lock(post_mutex_);
    batch.swap(posted_);
  }
  for (std::function<void()>& fn : batch) fn();
}

void WebSocketClientImpl::RunEventLoop() {
  while (!quit_) {
    RunPosted();
    if (quit_) break;

    // Drive the handshake (a no-op once the connect-only handle is done).
    int running_handles = 0;
    CURLMcode perform_code = curl_multi_perform(multi_.get(), &running_handles);
    int messages_left = 0;
    CURLMsg* message = nullptr;
    while ((message = curl_multi_info_read(multi_.get(), &messages_left)) !=
           nullptr) {
      if (message->msg == CURLMSG_DONE && message->easy_handle == easy_.get()) {
        OnHandshakeDone(message->data.result);
      }
    }
    if (perform_code != CURLM_OK && state_ == kStateConnecting) {
      // The multi handle itself is broken (not a transfer failure, which is
      // reported through curl_multi_info_read); give up on the handshake.
      logger_->LogDebug("curl_multi_perform failed for %s: %s", uri_.c_str(),
                        curl_multi_strerror(perform_code));
      CleanupEasyHandle();
      state_ = kStateClosed;
      OnError();
      continue;
    }

    // libcurl bounds only DNS/TCP/TLS for a connect-only handle, not the HTTP
    // Upgrade exchange, so enforce Connect()'s timeout here.
    int timeout_ms = wakeup_failed_ ? kPollTimeoutNoWakeupMs : kPollTimeoutMs;
    if (state_ == kStateConnecting &&
        handshake_deadline_ != std::chrono::steady_clock::time_point::max()) {
      const auto remaining_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              handshake_deadline_ - std::chrono::steady_clock::now())
              .count();
      if (remaining_ms <= 0) {
        logger_->LogDebug("WebSocket handshake to %s timed out", uri_.c_str());
        CleanupEasyHandle();
        state_ = kStateClosed;
        OnError();
        continue;
      }
      timeout_ms = static_cast<int>(
          std::min<int64_t>(timeout_ms, static_cast<int64_t>(remaining_ms)));
    }

    // Sleep until the handshake makes progress, the open socket is ready,
    // Post() wakes us, or the timeout elapses.
    curl_waitfd wait_fd;
    wait_fd.fd = socket_;
    wait_fd.events = CURL_WAIT_POLLIN;
    if (!send_queue_.empty()) wait_fd.events |= CURL_WAIT_POLLOUT;
    wait_fd.revents = 0;
    const bool watch_socket =
        state_ == kStateOpen && socket_ != CURL_SOCKET_BAD;
    // Frames may still be buffered inside libcurl/TLS after a capped receive
    // pass, and the socket will not report them as readable; poll without
    // blocking so they are drained right after posted work has had its turn.
    if (watch_socket && receive_backlog_) timeout_ms = 0;
    int num_fds = 0;
    CURLMcode poll_code =
        curl_multi_poll(multi_.get(), watch_socket ? &wait_fd : nullptr,
                        watch_socket ? 1 : 0, timeout_ms, &num_fds);
    if (poll_code != CURLM_OK) {
      // Never spin at 100% CPU on a persistently failing poll; this is rare
      // (e.g. fd exhaustion) and the sleep keeps Post() latency bounded.
      logger_->LogDebug("curl_multi_poll failed for %s: %s", uri_.c_str(),
                        curl_multi_strerror(poll_code));
      std::this_thread::sleep_for(
          std::chrono::milliseconds(kPollFailureBackoffMs));
      continue;
    }
    if (watch_socket && (wait_fd.revents != 0 || receive_backlog_)) {
      const bool backlog = receive_backlog_;
      receive_backlog_ = false;
      Pump(wait_fd.revents | (backlog ? CURL_WAIT_POLLIN : 0));
    }
  }
}

// ---------------------------------------------------------------------------
// Handshake.
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

  // Every option below is folded into setopt_error and checked once; a failure
  // in any of them means the handle is not configured as the SDK requires.
  CURLcode setopt_error = CURLE_OK;
  ApplyOpt(easy.get(), CURLOPT_URL, uri_.c_str(), &setopt_error);
  // Restrict schemes to ws/wss so system-libcurl builds that compile in extra
  // protocols (file://, http://, ftp://, ...) cannot open non-WebSocket URIs.
  ApplyOpt(easy.get(), CURLOPT_PROTOCOLS_STR, "ws,wss", &setopt_error);
  // CURLOPT_CONNECT_ONLY = 2L instructs libcurl to perform the HTTP/1.1
  // WebSocket Upgrade handshake and stop after receiving 101 Switching
  // Protocols so framing can be driven via curl_ws_recv() and curl_ws_send().
  ApplyOpt(easy.get(), CURLOPT_CONNECT_ONLY, 2L, &setopt_error);
  // Never let libcurl touch process-wide signal handlers from the I/O thread.
  ApplyOpt(easy.get(), CURLOPT_NOSIGNAL, 1L, &setopt_error);
  // Handle PING frames explicitly in ReceiveFrames() so standalone PINGs flush
  // their PONG response immediately without waiting for another frame.
  ApplyOpt(easy.get(), CURLOPT_WS_OPTIONS,
           static_cast<long>(CURLWS_NOAUTOPONG),  // NOLINT
           &setopt_error);
  if (timeout_ms > 0) {
    // Bounds DNS + TCP + TLS; the HTTP Upgrade exchange is bounded by
    // handshake_deadline_ in RunEventLoop().
    ApplyOpt(easy.get(), CURLOPT_CONNECTTIMEOUT_MS,
             static_cast<long>(timeout_ms),  // NOLINT
             &setopt_error);
  }
  if (setopt_error != CURLE_OK) {
    logger_->LogError("Failed to configure WebSocket handle for %s: (%d) %s",
                      uri_.c_str(), static_cast<int>(setopt_error),
                      curl_easy_strerror(setopt_error));
    OnError();
    return;
  }
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

  std::string app_check_token;
  {
    MutexLock lock(app_check_token_mutex_);
    app_check_token = app_check_token_;
  }

  auto append_header = [this](const char* header) {
    curl_slist* next = curl_slist_append(request_headers_.get(), header);
    if (next == nullptr) return false;
    request_headers_.release();
    request_headers_.reset(next);
    return true;
  };
  std::string user_agent_header = "User-Agent: " + user_agent_;
  if (!append_header(user_agent_header.c_str())) {
    OnError();
    return;
  }

  const char* api_client = App::GetUserAgent();
  if (api_client != nullptr && api_client[0] != '\0') {
    std::string api_client_header =
        std::string(app_common::kApiClientHeader) + ": " + api_client;
    if (!append_header(api_client_header.c_str())) {
      OnError();
      return;
    }
  }

  if (!app_check_token.empty()) {
    std::string app_check_header = "X-Firebase-AppCheck: " + app_check_token;
    if (!append_header(app_check_header.c_str())) {
      OnError();
      return;
    }
  }

  CURLcode header_result = CURLE_OK;
  ApplyOpt(easy.get(), CURLOPT_HTTPHEADER, request_headers_.get(),
           &header_result);
  if (header_result != CURLE_OK) {
    logger_->LogError("Failed to set WebSocket request headers for %s: (%d) %s",
                      uri_.c_str(), static_cast<int>(header_result),
                      curl_easy_strerror(header_result));
    request_headers_.reset();
    OnError();
    return;
  }

  easy_ = std::move(easy);
  incoming_message_buffer_.clear();
  incoming_ping_buffer_.clear();
  send_queue_.clear();
  frame_in_progress_ = false;
  current_send_offset_ = 0;

  CURLMcode mc = curl_multi_add_handle(multi_.get(), easy_.get());
  if (mc != CURLM_OK) {
    logger_->LogError("curl_multi_add_handle failed: %s",
                      curl_multi_strerror(mc));
    easy_.reset();
    request_headers_.reset();
    OnError();
    return;
  }
  state_ = kStateConnecting;
  handshake_deadline_ = timeout_ms > 0
                            ? std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(timeout_ms)
                            : std::chrono::steady_clock::time_point::max();
}

void WebSocketClientImpl::OnHandshakeDone(CURLcode result) {
  // The handle stays attached to multi_: libcurl closes a connect-only
  // connection the moment its handle is removed.
  if (result != CURLE_OK) {
    logger_->LogDebug(
        "Error occurred while establishing connection to %s: (%d) %s",
        uri_.c_str(), static_cast<int>(result), curl_easy_strerror(result));
    CleanupEasyHandle();
    state_ = kStateClosed;
    OnError();
    return;
  }

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
    // If a frame is already partially written the socket is backpressured;
    // RunEventLoop() polls for POLLOUT while send_queue_ is non-empty.
    if (!frame_in_progress_) FlushSendQueue();
  } else {
    logger_->LogWarning("Cannot send message.  websocket is not available");
  }
}

void WebSocketClientImpl::TeardownOnLoop() {
  if (state_ == kStateOpen && easy_ != nullptr && !frame_in_progress_) {
    size_t sent = 0;
    (void)curl_ws_send(easy_.get(), kCloseNormalPayload,
                       sizeof(kCloseNormalPayload), &sent, 0, CURLWS_CLOSE);
  }
  CleanupEasyHandle();
  state_ = kStateClosed;
}

void WebSocketClientImpl::Pump(int revents) {
  if (state_ != kStateOpen) return;
  // Only (re)enter curl_ws_send() when the socket signalled writability or
  // nothing is mid-frame: a POLLIN wakeup (server PING, data) while a frame is
  // backpressured has nothing to flush yet.
  if (!frame_in_progress_ || (revents & CURL_WAIT_POLLOUT) != 0) {
    FlushSendQueue();
  }
  // Receiving is always attempted: curl_ws_recv() is non-blocking and TLS may
  // hold already-decrypted frames that the socket no longer reports as
  // readable.
  if (state_ == kStateOpen) ReceiveFrames();
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
      // Ordinary network drops end up here; the handler decides whether to
      // reconnect, so log at the same level as the curl_ws_recv() failure path.
      logger_->LogDebug("curl_ws_send failed for %s: (%d) %s", uri_.c_str(),
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

  int frames_this_pass = 0;
  while (state_ == kStateOpen) {
    if (frames_this_pass++ >= kMaxFramesPerReceivePass) {
      // Yield to RunEventLoop() so posted work runs; it polls again without
      // blocking because receive_backlog_ is set.
      receive_backlog_ = true;
      break;
    }
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

    const uint64_t frame_remaining =
        meta->bytesleft > 0 ? static_cast<uint64_t>(meta->bytesleft) : 0;

    if (meta->flags & CURLWS_PING) {
      // libcurl already rejects fragmented and >125-byte control frames
      // (ws.c); these checks are kept as a backstop for other libcurl builds
      // and so incoming_ping_buffer_ has an explicit bound.
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
      if (rlen > kMaxMessagePayloadBytes - incoming_message_buffer_.size() ||
          frame_remaining > kMaxMessagePayloadBytes -
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
  socket_ = CURL_SOCKET_BAD;
  if (easy_ != nullptr) {
    // Detaching aborts an in-flight handshake or closes the open connection.
    curl_multi_remove_handle(multi_.get(), easy_.get());
    easy_.reset();
  }
  request_headers_.reset();
  incoming_message_buffer_.clear();
  incoming_ping_buffer_.clear();
  send_queue_.clear();
  frame_in_progress_ = false;
  current_send_offset_ = 0;
  receive_backlog_ = false;
}

bool WebSocketClientImpl::IsWebSocketAvailable() const {
  return state_ == kStateOpen && easy_ != nullptr;
}

// ---------------------------------------------------------------------------
// Handler notifications, marshalled to the Database scheduler.
// ---------------------------------------------------------------------------

void WebSocketClientImpl::ScheduleHandlerEvent(
    std::function<void(WebSocketClientEventHandler*)> fn) {
  if (handler_ == nullptr) return;
  ClientRef client_ref = safe_this_;
  scheduler_->Schedule([client_ref, fn = std::move(fn)]() mutable {
    ClientRefLock lock(&client_ref);
    WebSocketClientImpl* client = lock.GetReference();
    if (client != nullptr && client->handler_ != nullptr) {
      fn(client->handler_);
    }
  });
}

void WebSocketClientImpl::OnError() {
  std::string uri = uri_;
  ScheduleHandlerEvent(
      [uri = std::move(uri)](WebSocketClientEventHandler* handler) {
        WebSocketClientErrorData error(uri.c_str());
        handler->OnError(error);
      });
}

void WebSocketClientImpl::OnConnection() {
  ScheduleHandlerEvent(
      [](WebSocketClientEventHandler* handler) { handler->OnOpen(); });
}

void WebSocketClientImpl::OnMessage(const char* message, size_t length) {
  std::string payload(message, length);
  ScheduleHandlerEvent(
      [payload = std::move(payload)](WebSocketClientEventHandler* handler) {
        handler->OnMessage(payload.c_str());
      });
}

void WebSocketClientImpl::OnDisconnection() {
  ScheduleHandlerEvent(
      [](WebSocketClientEventHandler* handler) { handler->OnClose(); });
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
