/*
 * Copyright 2026 Google LLC
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketFd = SOCKET;
constexpr SocketFd kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketFd = int;
constexpr SocketFd kInvalidSocket = -1;
#endif

#include "app/http/http_transport.h"
#include "gtest/gtest.h"

namespace firebase {
namespace http {
namespace {

void EnsureSocketSubsystemInitialized() {
#if defined(_WIN32)
  static std::once_flag wsa_once;
  std::call_once(wsa_once, []() {
    WSADATA data{};
    WSAStartup(MAKEWORD(2, 2), &data);
  });
#else
  static std::once_flag sigpipe_once;
  std::call_once(sigpipe_once, []() { std::signal(SIGPIPE, SIG_IGN); });
#endif
}

void CloseSocket(SocketFd fd) {
  if (fd == kInvalidSocket) return;
#if defined(_WIN32)
  closesocket(fd);
#else
  close(fd);
#endif
}

void ShutdownWrite(SocketFd fd) {
  if (fd == kInvalidSocket) return;
#if defined(_WIN32)
  shutdown(fd, SD_SEND);
#else
  shutdown(fd, SHUT_WR);
#endif
}

bool WaitForSocketReadable(SocketFd fd, int timeout_ms) {
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);
  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
#if defined(_WIN32)
  int ready = select(0, &read_fds, nullptr, nullptr, &tv);
#else
  int ready = select(fd + 1, &read_fds, nullptr, nullptr, &tv);
#endif
  return ready > 0;
}

bool WaitForPeerClose(SocketFd fd, int timeout_ms) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  char drain[4096];
  while (true) {
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return false;
    }
    int rem_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count());
    if (rem_ms <= 0) rem_ms = 1;
    if (!WaitForSocketReadable(fd, rem_ms)) {
      return false;
    }
#if defined(_WIN32)
    int n = recv(fd, drain, sizeof(drain), 0);
#else
    ssize_t n = recv(fd, drain, sizeof(drain), 0);
#endif
    if (n <= 0) {
      return true;
    }
  }
}

bool SendAll(SocketFd fd, const char* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
#if defined(_WIN32)
    int n = send(fd, data + sent, static_cast<int>(len - sent), 0);
#else
#ifdef MSG_NOSIGNAL
    constexpr int kSendFlags = MSG_NOSIGNAL;
#else
    constexpr int kSendFlags = 0;
#endif
    ssize_t n = send(fd, data + sent, len - sent, kSendFlags);
#endif
    if (n <= 0) {
      return false;
    }
    sent += static_cast<size_t>(n);
  }
  return true;
}

bool SendAll(SocketFd fd, std::string_view s) {
  return SendAll(fd, s.data(), s.size());
}

struct ParsedRequest {
  std::string method;
  std::string target;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;

  std::string GetHeader(std::string_view name) const {
    for (const auto& kv : headers) {
      if (AsciiEqualsIgnoreCase(kv.first, name)) {
        return kv.second;
      }
    }
    return {};
  }

  bool HasHeader(std::string_view name) const {
    for (const auto& kv : headers) {
      if (AsciiEqualsIgnoreCase(kv.first, name)) {
        return true;
      }
    }
    return false;
  }
};

// Minimal in-process HTTP/1.1 loopback test server for deterministic HTTP & SSE
// integration testing (supports chunked uploads, SSE trickle streams, idle
// stalls, redirects, and concurrent connections).
class LoopbackHttpServer {
 public:
  using Handler = std::function<void(const ParsedRequest&, SocketFd)>;

  explicit LoopbackHttpServer(Handler handler) : handler_(std::move(handler)) {
    EnsureSocketSubsystemInitialized();
    SocketFd fd = socket(AF_INET, SOCK_STREAM, 0);
    listen_fd_.store(fd, std::memory_order_release);
    EXPECT_NE(fd, kInvalidSocket);

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    EXPECT_EQ(bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    EXPECT_EQ(listen(fd, 32), 0);

    sockaddr_in bound{};
#if defined(_WIN32)
    int bound_len = sizeof(bound);
#else
    socklen_t bound_len = sizeof(bound);
#endif
    EXPECT_EQ(getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &bound_len),
              0);
    port_ = ntohs(bound.sin_port);

    accept_thread_ = std::thread([this]() { AcceptLoop(); });
  }

  ~LoopbackHttpServer() { Stop(); }

  void Stop() {
    if (stopped_.exchange(true)) {
      return;
    }
    if (port_ > 0) {
      SocketFd wake = socket(AF_INET, SOCK_STREAM, 0);
      if (wake != kInvalidSocket) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(static_cast<uint16_t>(port_));
        (void)connect(wake, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        CloseSocket(wake);
      }
    }
    if (accept_thread_.joinable()) {
      accept_thread_.join();
    }
    SocketFd to_close =
        listen_fd_.exchange(kInvalidSocket, std::memory_order_acq_rel);
    if (to_close != kInvalidSocket) {
      CloseSocket(to_close);
    }
    std::vector<std::thread> to_join;
    {
      std::lock_guard<std::mutex> lock(workers_mutex_);
      to_join.swap(workers_);
    }
    for (auto& t : to_join) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  int port() const { return port_; }

  std::string Url(std::string_view path) const {
    return "http://127.0.0.1:" + std::to_string(port_) + std::string(path);
  }

 private:
  void AcceptLoop() {
    while (!stopped_.load()) {
      SocketFd fd = listen_fd_.load(std::memory_order_acquire);
      if (fd == kInvalidSocket) {
        break;
      }
      if (!WaitForSocketReadable(fd, 50)) {
        continue;
      }
      if (stopped_.load()) {
        break;
      }
      sockaddr_in client_addr{};
#if defined(_WIN32)
      int len = sizeof(client_addr);
#else
      socklen_t len = sizeof(client_addr);
#endif
      SocketFd client =
          accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &len);
      if (client == kInvalidSocket) {
        if (stopped_.load()) break;
        continue;
      }
      if (stopped_.load()) {
        CloseSocket(client);
        break;
      }
      int nodelay = 1;
      setsockopt(client, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
#ifdef SO_NOSIGPIPE
      int nosigpipe = 1;
      setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE,
                 reinterpret_cast<const char*>(&nosigpipe), sizeof(nosigpipe));
#endif
      std::lock_guard<std::mutex> lock(workers_mutex_);
      workers_.emplace_back([this, client]() {
        HandleClient(client);
        CloseSocket(client);
      });
    }
  }

  static bool ReadHttpRequest(SocketFd fd, ParsedRequest* out) {
    std::string buf;
    char tmp[4096];
    size_t header_end = std::string::npos;
    while (header_end == std::string::npos) {
#if defined(_WIN32)
      int n = recv(fd, tmp, sizeof(tmp), 0);
#else
      ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
#endif
      if (n <= 0) return false;
      buf.append(tmp, static_cast<size_t>(n));
      header_end = buf.find("\r\n\r\n");
    }

    std::string header_block = buf.substr(0, header_end);
    std::string remainder = buf.substr(header_end + 4);

    size_t line_end = header_block.find("\r\n");
    std::string req_line = line_end == std::string::npos
                               ? header_block
                               : header_block.substr(0, line_end);
    size_t sp1 = req_line.find(' ');
    size_t sp2 = sp1 == std::string::npos ? std::string::npos
                                          : req_line.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) {
      return false;
    }
    out->method = req_line.substr(0, sp1);
    out->target = req_line.substr(sp1 + 1, sp2 - (sp1 + 1));

    size_t pos =
        line_end == std::string::npos ? header_block.size() : line_end + 2;
    while (pos < header_block.size()) {
      size_t next = header_block.find("\r\n", pos);
      std::string_view line =
          next == std::string::npos
              ? std::string_view(header_block).substr(pos)
              : std::string_view(header_block).substr(pos, next - pos);
      pos = next == std::string::npos ? header_block.size() : next + 2;
      size_t colon = line.find(':');
      if (colon != std::string_view::npos && colon > 0) {
        out->headers.emplace_back(
            std::string(TrimAsciiWhitespace(line.substr(0, colon))),
            std::string(TrimAsciiWhitespace(line.substr(colon + 1))));
      }
    }

    std::string te = out->GetHeader("Transfer-Encoding");
    if (AsciiEqualsIgnoreCase(te, "chunked")) {
      std::string raw = std::move(remainder);
      auto ensure = [&](size_t need) -> bool {
        while (raw.size() < need) {
#if defined(_WIN32)
          int n = recv(fd, tmp, sizeof(tmp), 0);
#else
          ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
#endif
          if (n <= 0) return false;
          raw.append(tmp, static_cast<size_t>(n));
        }
        return true;
      };
      while (true) {
        size_t crlf = raw.find("\r\n");
        while (crlf == std::string::npos) {
          if (!ensure(raw.size() + 1)) return false;
          crlf = raw.find("\r\n");
        }
        size_t chunk_len = static_cast<size_t>(
            std::strtoull(raw.substr(0, crlf).c_str(), nullptr, 16));
        raw.erase(0, crlf + 2);
        if (chunk_len == 0) {
          ensure(2);
          break;
        }
        if (!ensure(chunk_len + 2)) return false;
        out->body.append(raw.data(), chunk_len);
        raw.erase(0, chunk_len + 2);
      }
    } else {
      std::string cl = out->GetHeader("Content-Length");
      size_t content_len =
          cl.empty()
              ? 0
              : static_cast<size_t>(std::strtoull(cl.c_str(), nullptr, 10));
      out->body = std::move(remainder);
      while (out->body.size() < content_len) {
#if defined(_WIN32)
        int n = recv(fd, tmp, sizeof(tmp), 0);
#else
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
#endif
        if (n <= 0) return false;
        out->body.append(tmp, static_cast<size_t>(n));
      }
      if (out->body.size() > content_len) {
        out->body.resize(content_len);
      }
    }
    return true;
  }

  void HandleClient(SocketFd fd) {
    ParsedRequest req;
    if (!ReadHttpRequest(fd, &req)) {
      return;
    }
    handler_(req, fd);
  }

  Handler handler_;
  std::atomic<SocketFd> listen_fd_{kInvalidSocket};
  int port_ = 0;
  std::atomic<bool> stopped_{false};
  std::thread accept_thread_;
  std::mutex workers_mutex_;
  std::vector<std::thread> workers_;
};

// Event-driven test helper that asserts exact completion events using blocking
// `HttpWaitNextEventInternal` calls (never polling in a sleep loop, T2).
class BlockingEventDemux {
 public:
  void DrainStale() {
    buffered_.clear();
    while (HttpWaitNextEventInternal(0) != 0) {
    }
  }

  bool WaitFor(int64_t handle, HttpEventKind expected_kind, int timeout_ms) {
    auto& q = buffered_[handle];
    for (auto it = q.begin(); it != q.end(); ++it) {
      if (*it == expected_kind) {
        q.erase(it);
        return true;
      }
    }

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    while (true) {
      auto now = std::chrono::steady_clock::now();
      if (now >= deadline) {
        return false;
      }
      int32_t rem_ms = static_cast<int32_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
              .count());
      if (rem_ms <= 0) {
        rem_ms = 1;
      }
      int64_t ev = HttpWaitNextEventInternal(rem_ms);
      if (ev == 0) {
        return false;
      }
      int64_t ev_handle = ev >> 8;
      auto ev_kind = static_cast<HttpEventKind>(ev & 0xFF);
      if (ev_handle == handle && ev_kind == expected_kind) {
        return true;
      }
      buffered_[ev_handle].push_back(ev_kind);
    }
  }

 private:
  std::unordered_map<int64_t, std::deque<HttpEventKind>> buffered_;
};

int32_t WaitHeaders(BlockingEventDemux* demux, int64_t handle,
                    int timeout_ms = 5000) {
  int32_t status = HttpPollResponseHeadersInternal(handle);
  if (status != 0) {
    return status;
  }
  EXPECT_TRUE(
      demux->WaitFor(handle, kHttpEventResponseHeadersReady, timeout_ms))
      << "Timed out waiting for kHttpEventResponseHeadersReady on handle "
      << handle;
  return HttpPollResponseHeadersInternal(handle);
}

bool ReadAllBody(BlockingEventDemux* demux, int64_t handle, std::string* out,
                 int timeout_ms = 10000) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  char buf[16384];
  while (true) {
    int32_t n = HttpTryReadResponseBodyInternal(handle, buf, sizeof(buf));
    if (n > 0) {
      out->append(buf, static_cast<size_t>(n));
      continue;
    }
    if (n == 0) {
      return true;
    }
    if (n == kHttpReadErrorOrCanceled) {
      return false;
    }
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return false;
    }
    int rem_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count());
    if (!demux->WaitFor(handle, kHttpEventResponseBodyAvailable,
                        (std::max)(rem_ms, 1))) {
      ADD_FAILURE() << "Expected kHttpEventResponseBodyAvailable event";
      return false;
    }
  }
}

bool WriteAllBody(BlockingEventDemux* demux, int64_t handle,
                  const uint8_t* data, size_t len, int timeout_ms = 10000) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  size_t offset = 0;
  while (offset < len) {
    int32_t to_send = static_cast<int32_t>(
        (std::min)(len - offset, static_cast<size_t>(65536)));
    int32_t n = HttpTryWriteRequestBodyInternal(
        handle, const_cast<uint8_t*>(data + offset), to_send);
    if (n < 0) return false;
    if (n > 0) {
      offset += static_cast<size_t>(n);
      continue;
    }
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return false;
    }
    int rem_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count());
    if (!demux->WaitFor(handle, kHttpEventRequestSpaceAvailable,
                        (std::max)(rem_ms, 1))) {
      ADD_FAILURE() << "Expected kHttpEventRequestSpaceAvailable event";
      return false;
    }
  }
  HttpFinishRequestBodyInternal(handle);
  return true;
}

TEST(HttpTransportCurlTest, GetBasic200AndMultiValueHeaders) {
  BlockingEventDemux demux;
  demux.DrainStale();

  LoopbackHttpServer server([](const ParsedRequest& req, SocketFd fd) {
    EXPECT_EQ(req.method, "GET");
    EXPECT_EQ(req.target, "/hello");
    EXPECT_EQ(req.GetHeader("X-Client"), "firebase-cpp");
    SendAll(fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Set-Cookie: a=1\r\n"
            "Set-Cookie: b=2\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "world");
  });

  int64_t h = HttpCreateRequestInternal("GET", server.Url("/hello").c_str(),
                                        "X-Client\tfirebase-cpp\n", "",
                                        kNoRequestContent);
  HttpStartRequestInternal(h);
  ASSERT_EQ(WaitHeaders(&demux, h), 200);

  std::string raw_headers = HttpGetResponseHeadersInternal(h);
  EXPECT_NE(raw_headers.find("Set-Cookie\ta=1\n"), std::string::npos);
  EXPECT_NE(raw_headers.find("Set-Cookie\tb=2\n"), std::string::npos);

  std::string body;
  ASSERT_TRUE(ReadAllBody(&demux, h, &body));
  EXPECT_EQ(body, "world");
  HttpDestroyRequestInternal(h);
}

TEST(HttpTransportCurlTest, ZeroLengthGetAndHeadStayGetAndHead) {
  BlockingEventDemux demux;
  demux.DrainStale();

  std::mutex mu;
  std::string get_method;
  std::string head_method;
  LoopbackHttpServer server([&](const ParsedRequest& req, SocketFd fd) {
    if (req.target == "/zero-get") {
      {
        std::lock_guard<std::mutex> lock(mu);
        get_method = req.method;
      }
      SendAll(fd,
              "HTTP/1.1 200 OK\r\n"
              "Content-Length: 2\r\n"
              "Connection: close\r\n\r\nok");
    } else if (req.target == "/zero-head") {
      {
        std::lock_guard<std::mutex> lock(mu);
        head_method = req.method;
      }
      SendAll(fd,
              "HTTP/1.1 200 OK\r\n"
              "Content-Length: 1234\r\n"
              "Connection: close\r\n\r\n");
    }
  });

  int64_t h1 = HttpCreateRequestInternal("GET", server.Url("/zero-get").c_str(),
                                         "", "", 0);
  HttpStartRequestInternal(h1);
  ASSERT_EQ(WaitHeaders(&demux, h1), 200);
  std::string b1;
  ASSERT_TRUE(ReadAllBody(&demux, h1, &b1));
  EXPECT_EQ(b1, "ok");
  HttpDestroyRequestInternal(h1);

  int64_t h2 = HttpCreateRequestInternal(
      "HEAD", server.Url("/zero-head").c_str(), "", "", 0);
  HttpStartRequestInternal(h2);
  ASSERT_EQ(WaitHeaders(&demux, h2), 200);
  std::string b2;
  ASSERT_TRUE(ReadAllBody(&demux, h2, &b2));
  EXPECT_TRUE(b2.empty());
  HttpDestroyRequestInternal(h2);

  std::lock_guard<std::mutex> lock(mu);
  EXPECT_EQ(get_method, "GET");
  EXPECT_EQ(head_method, "HEAD");
}

TEST(HttpTransportCurlTest, PostJsonAndError400And204NoContent) {
  BlockingEventDemux demux;
  demux.DrainStale();

  LoopbackHttpServer server([](const ParsedRequest& req, SocketFd fd) {
    if (req.target == "/callable") {
      EXPECT_EQ(req.method, "POST");
      EXPECT_EQ(req.body, "{\"data\":{\"msg\":\"hi\"}}");
      std::string resp = "{\"result\":{\"echo\":\"hi\"}}";
      SendAll(fd,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: " +
                  std::to_string(resp.size()) +
                  "\r\nConnection: close\r\n\r\n" + resp);
    } else if (req.target == "/err400") {
      std::string resp = "{\"error\":{\"status\":\"INVALID_ARGUMENT\"}}";
      SendAll(fd,
              "HTTP/1.1 400 Bad Request\r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: " +
                  std::to_string(resp.size()) +
                  "\r\nConnection: close\r\n\r\n" + resp);
    } else if (req.target == "/delete") {
      EXPECT_EQ(req.method, "DELETE");
      SendAll(fd,
              "HTTP/1.1 204 No Content\r\n"
              "Connection: close\r\n\r\n");
    }
  });

  // 1. JSON POST
  const std::string payload = "{\"data\":{\"msg\":\"hi\"}}";
  int64_t h1 =
      HttpCreateRequestInternal("POST", server.Url("/callable").c_str(),
                                "Content-Type\tapplication/json\n", "",
                                static_cast<int64_t>(payload.size()));
  HttpStartRequestInternal(h1);
  ASSERT_TRUE(WriteAllBody(&demux, h1,
                           reinterpret_cast<const uint8_t*>(payload.data()),
                           payload.size()));
  ASSERT_EQ(WaitHeaders(&demux, h1), 200);
  std::string body1;
  ASSERT_TRUE(ReadAllBody(&demux, h1, &body1));
  EXPECT_EQ(body1, "{\"result\":{\"echo\":\"hi\"}}");
  HttpDestroyRequestInternal(h1);

  // 2. 400 error JSON body preserved
  int64_t h2 = HttpCreateRequestInternal("POST", server.Url("/err400").c_str(),
                                         "", "", 0);
  HttpStartRequestInternal(h2);
  ASSERT_EQ(WaitHeaders(&demux, h2), 400);
  std::string body2;
  ASSERT_TRUE(ReadAllBody(&demux, h2, &body2));
  EXPECT_EQ(body2, "{\"error\":{\"status\":\"INVALID_ARGUMENT\"}}");
  HttpDestroyRequestInternal(h2);

  // 3. 204 No Content
  int64_t h3 = HttpCreateRequestInternal(
      "DELETE", server.Url("/delete").c_str(), "", "", kNoRequestContent);
  HttpStartRequestInternal(h3);
  ASSERT_EQ(WaitHeaders(&demux, h3), 204);
  std::string body3;
  ASSERT_TRUE(ReadAllBody(&demux, h3, &body3));
  EXPECT_TRUE(body3.empty());
  HttpDestroyRequestInternal(h3);
}

TEST(HttpTransportCurlTest, SseTrickleStreamingImmediateDelivery) {
  BlockingEventDemux demux;
  demux.DrainStale();

  std::mutex gate_mu;
  std::condition_variable gate_cv;
  int client_acked_chunk = 0;

  LoopbackHttpServer server([&](const ParsedRequest& req, SocketFd fd) {
    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.GetHeader("Accept"), "text/event-stream");
    SendAll(fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: close\r\n"
            "\r\n");

    auto send_chunk = [&](std::string_view sse) {
      char hex[32];
      std::snprintf(hex, sizeof(hex), "%zx\r\n", sse.size());
      SendAll(fd, hex);
      SendAll(fd, sse);
      SendAll(fd, "\r\n");
    };

    send_chunk("data: {\"seq\":1}\n\n");
    {
      std::unique_lock<std::mutex> lock(gate_mu);
      ASSERT_TRUE(gate_cv.wait_for(lock, std::chrono::seconds(5),
                                   [&]() { return client_acked_chunk >= 1; }));
    }

    send_chunk("data: {\"seq\":2}\n\n");
    {
      std::unique_lock<std::mutex> lock(gate_mu);
      ASSERT_TRUE(gate_cv.wait_for(lock, std::chrono::seconds(5),
                                   [&]() { return client_acked_chunk >= 2; }));
    }

    send_chunk("data: {\"seq\":3}\n\n");
    SendAll(fd, "0\r\n\r\n");
  });

  const std::string req_body = "{\"prompt\":\"stream\"}";
  int64_t h = HttpCreateRequestInternal(
      "POST", server.Url("/sse").c_str(),
      "Accept\ttext/event-stream\nContent-Type\tapplication/json\n", "",
      static_cast<int64_t>(req_body.size()));
  HttpStartRequestInternal(h);
  ASSERT_TRUE(WriteAllBody(&demux, h,
                           reinterpret_cast<const uint8_t*>(req_body.data()),
                           req_body.size()));
  ASSERT_EQ(WaitHeaders(&demux, h), 200);

  auto read_until = [&](std::string* accum, std::string_view needle) -> bool {
    char buf[1024];
    while (accum->find(needle) == std::string::npos) {
      int32_t n = HttpTryReadResponseBodyInternal(h, buf, sizeof(buf));
      if (n > 0) {
        accum->append(buf, static_cast<size_t>(n));
      } else if (n == 0 || n == kHttpReadErrorOrCanceled) {
        break;
      } else {
        if (!demux.WaitFor(h, kHttpEventResponseBodyAvailable, 5000)) {
          return false;
        }
      }
    }
    return accum->find(needle) != std::string::npos;
  };

  std::string accum;
  ASSERT_TRUE(read_until(&accum, "data: {\"seq\":1}\n\n"));
  {
    std::lock_guard<std::mutex> lock(gate_mu);
    client_acked_chunk = 1;
  }
  gate_cv.notify_all();

  ASSERT_TRUE(read_until(&accum, "data: {\"seq\":2}\n\n"));
  {
    std::lock_guard<std::mutex> lock(gate_mu);
    client_acked_chunk = 2;
  }
  gate_cv.notify_all();

  std::string rest;
  ASSERT_TRUE(ReadAllBody(&demux, h, &rest));
  accum.append(rest);
  EXPECT_NE(accum.find("data: {\"seq\":3}\n\n"), std::string::npos);
  HttpDestroyRequestInternal(h);
}

TEST(HttpTransportCurlTest, SseIdleCancelUnblocksPromptly) {
  BlockingEventDemux demux;
  demux.DrainStale();

  // T1: Server explicitly waits for peer socket close (EOF/RST) on cancel.
  std::mutex close_mu;
  std::condition_variable close_cv;
  bool server_observed_close = false;

  LoopbackHttpServer server([&](const ParsedRequest&, SocketFd fd) {
    SendAll(fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "12\r\ndata: {\"ok\":true}\n\r\n");
    bool closed = WaitForPeerClose(fd, 5000);
    {
      std::lock_guard<std::mutex> lock(close_mu);
      server_observed_close = closed;
    }
    close_cv.notify_all();
  });

  int64_t h = HttpCreateRequestInternal("GET", server.Url("/hang").c_str(),
                                        "Accept\ttext/event-stream\n", "",
                                        kNoRequestContent);
  HttpStartRequestInternal(h);
  ASSERT_EQ(WaitHeaders(&demux, h), 200);

  // Read the first SSE chunk using blocking event waits (T2).
  std::string got;
  char buf[512];
  while (got.empty()) {
    int32_t n = HttpTryReadResponseBodyInternal(h, buf, sizeof(buf));
    if (n > 0) {
      got.append(buf, static_cast<size_t>(n));
    } else {
      ASSERT_EQ(n, kHttpReadWouldBlock);
      ASSERT_TRUE(demux.WaitFor(h, kHttpEventResponseBodyAvailable, 5000));
    }
  }
  EXPECT_EQ(got, "data: {\"ok\":true}\n");

  // Arm a read while server is silent, then signal canceler via cv (T3: no
  // sleep-based synchronization).
  ASSERT_EQ(HttpTryReadResponseBodyInternal(h, buf, sizeof(buf)),
            kHttpReadWouldBlock);
  std::mutex cancel_mu;
  std::condition_variable cancel_cv;
  bool armed = false;
  std::thread canceler([&]() {
    {
      std::unique_lock<std::mutex> lock(cancel_mu);
      cancel_cv.wait(lock, [&]() { return armed; });
    }
    HttpCancelRequestInternal(h);
  });

  auto t0 = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(cancel_mu);
    armed = true;
  }
  cancel_cv.notify_one();

  ASSERT_TRUE(demux.WaitFor(h, kHttpEventResponseBodyAvailable, 3000));
  int32_t rc = HttpTryReadResponseBodyInternal(h, buf, sizeof(buf));
  canceler.join();

  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
  EXPECT_EQ(rc, kHttpReadErrorOrCanceled);
  EXPECT_EQ(HttpGetErrorClassInternal(h), kHttpErrorCanceled);
  EXPECT_LT(elapsed_ms, 1000);

  // Assert the TCP socket actually closed from HttpCancelRequestInternal alone,
  // BEFORE HttpDestroyRequestInternal is called (T1).
  {
    std::unique_lock<std::mutex> lock(close_mu);
    EXPECT_TRUE(close_cv.wait_for(lock, std::chrono::seconds(3),
                                  [&]() { return server_observed_close; }))
        << "Server did not observe TCP socket closure after cancel";
  }

  HttpDestroyRequestInternal(h);
}

TEST(HttpTransportCurlTest,
     RedirectsSameOriginPreservesAuthAndCrossOriginStrips) {
  BlockingEventDemux demux;
  demux.DrainStale();

  std::string observed_b_auth;
  std::string observed_b_cookie;
  std::string observed_b_appcheck;
  std::string observed_b_apikey;
  std::string observed_b_safe;
  std::string observed_b_method;
  std::mutex obs_mu;

  LoopbackHttpServer server_b([&](const ParsedRequest& req, SocketFd fd) {
    {
      std::lock_guard<std::mutex> lock(obs_mu);
      observed_b_method = req.method;
      observed_b_auth = req.GetHeader("Authorization");
      observed_b_cookie = req.GetHeader("Cookie");
      observed_b_appcheck = req.GetHeader("X-Firebase-AppCheck");
      observed_b_apikey = req.GetHeader("x-goog-api-key");
      observed_b_safe = req.GetHeader("X-Safe");
    }
    SendAll(fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 4\r\n"
            "Connection: close\r\n\r\ndone");
  });

  std::string observed_same_origin_auth;
  std::string observed_same_origin_method;
  std::string observed_301_method;
  std::string observed_303_post_method;
  std::string observed_303_head_method;
  std::string observed_space_target;
  LoopbackHttpServer server_a([&](const ParsedRequest& req, SocketFd fd) {
    if (req.target == "/same-302") {
      SendAll(fd,
              "HTTP/1.1 302 Found\r\n"
              "Location: /same-target\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    } else if (req.target == "/same-target") {
      {
        std::lock_guard<std::mutex> lock(obs_mu);
        observed_same_origin_method = req.method;
        observed_same_origin_auth = req.GetHeader("Authorization");
      }
      SendAll(fd,
              "HTTP/1.1 200 OK\r\n"
              "Content-Length: 2\r\n"
              "Connection: close\r\n\r\nok");
    } else if (req.target == "/post-301") {
      SendAll(fd,
              "HTTP/1.1 301 Moved Permanently\r\n"
              "Location: /after-301\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    } else if (req.target == "/after-301") {
      {
        std::lock_guard<std::mutex> lock(obs_mu);
        observed_301_method = req.method;
      }
      SendAll(fd,
              "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: "
              "close\r\n\r\n301");
    } else if (req.target == "/post-303") {
      SendAll(fd,
              "HTTP/1.1 303 See Other\r\n"
              "Location: /after-303-post\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    } else if (req.target == "/after-303-post") {
      {
        std::lock_guard<std::mutex> lock(obs_mu);
        observed_303_post_method = req.method;
      }
      SendAll(fd,
              "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: "
              "close\r\n\r\n303");
    } else if (req.target == "/head-303") {
      SendAll(fd,
              "HTTP/1.1 303 See Other\r\n"
              "Location: /after-303-head\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    } else if (req.target == "/after-303-head") {
      {
        std::lock_guard<std::mutex> lock(obs_mu);
        observed_303_head_method = req.method;
      }
      SendAll(fd,
              "HTTP/1.1 200 OK\r\nContent-Length: 42\r\nConnection: "
              "close\r\n\r\n");
    } else if (req.target == "/body-307") {
      SendAll(fd,
              "HTTP/1.1 307 Temporary Redirect\r\n"
              "Location: /never-reached\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    } else if (req.target == "/space-redir") {
      SendAll(fd,
              "HTTP/1.1 302 Found\r\n"
              "Location: /path with space\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    } else if (req.target == "/path%20with%20space") {
      {
        std::lock_guard<std::mutex> lock(obs_mu);
        observed_space_target = req.target;
      }
      SendAll(fd,
              "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: "
              "close\r\n\r\nspace");
    } else if (req.target == "/cross-307") {
      SendAll(fd,
              "HTTP/1.1 307 Temporary Redirect\r\n"
              "Location: " +
                  server_b.Url("/final") +
                  "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    } else if (req.target == "/loop") {
      SendAll(fd,
              "HTTP/1.1 302 Found\r\n"
              "Location: /loop\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    } else if (req.target == "/unsafe") {
      SendAll(fd,
              "HTTP/1.1 302 Found\r\n"
              "Location: file:///etc/passwd\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    }
  });

  // 1. Same-origin 302 POST -> GET rewrites method and preserves Authorization.
  int64_t h1 = HttpCreateRequestInternal(
      "POST", server_a.Url("/same-302").c_str(),
      "Authorization\tBearer secret\nContent-Type\tapplication/json\n", "", 0);
  HttpStartRequestInternal(h1);
  ASSERT_EQ(WaitHeaders(&demux, h1), 200);
  std::string b1;
  ASSERT_TRUE(ReadAllBody(&demux, h1, &b1));
  EXPECT_EQ(b1, "ok");
  {
    std::lock_guard<std::mutex> lock(obs_mu);
    EXPECT_EQ(observed_same_origin_method, "GET");
    EXPECT_EQ(observed_same_origin_auth, "Bearer secret");
  }
  HttpDestroyRequestInternal(h1);

  // 2. Cross-origin 307 strips sensitive headers (including Cookie, S3),
  // forwards safe headers.
  int64_t h2 =
      HttpCreateRequestInternal("GET", server_a.Url("/cross-307").c_str(),
                                "Authorization\tBearer secret\n"
                                "Cookie\tsession=xyz\n"
                                "X-Firebase-AppCheck\tappcheck-tok\n"
                                "x-goog-api-key\tapi-key\n"
                                "X-Safe\tkeep-me\n",
                                "", kNoRequestContent);
  HttpStartRequestInternal(h2);
  ASSERT_EQ(WaitHeaders(&demux, h2), 200);
  std::string b2;
  ASSERT_TRUE(ReadAllBody(&demux, h2, &b2));
  EXPECT_EQ(b2, "done");
  {
    std::lock_guard<std::mutex> lock(obs_mu);
    EXPECT_EQ(observed_b_method, "GET");
    EXPECT_TRUE(observed_b_auth.empty());
    EXPECT_TRUE(observed_b_cookie.empty());
    EXPECT_TRUE(observed_b_appcheck.empty());
    EXPECT_TRUE(observed_b_apikey.empty());
    EXPECT_EQ(observed_b_safe, "keep-me");
  }
  HttpDestroyRequestInternal(h2);

  // 3. 301 POST -> GET and 303 POST -> GET (T4).
  int64_t h_301 = HttpCreateRequestInternal(
      "POST", server_a.Url("/post-301").c_str(), "", "", 0);
  HttpStartRequestInternal(h_301);
  ASSERT_EQ(WaitHeaders(&demux, h_301), 200);
  std::string b_301;
  ASSERT_TRUE(ReadAllBody(&demux, h_301, &b_301));
  EXPECT_EQ(b_301, "301");
  HttpDestroyRequestInternal(h_301);

  int64_t h_303 = HttpCreateRequestInternal(
      "POST", server_a.Url("/post-303").c_str(), "", "", 0);
  HttpStartRequestInternal(h_303);
  ASSERT_EQ(WaitHeaders(&demux, h_303), 200);
  std::string b_303;
  ASSERT_TRUE(ReadAllBody(&demux, h_303, &b_303));
  EXPECT_EQ(b_303, "303");
  HttpDestroyRequestInternal(h_303);

  // 4. HEAD on 303 must stay HEAD (C1, T4).
  int64_t h_head = HttpCreateRequestInternal(
      "HEAD", server_a.Url("/head-303").c_str(), "", "", kNoRequestContent);
  HttpStartRequestInternal(h_head);
  ASSERT_EQ(WaitHeaders(&demux, h_head), 200);
  std::string b_head;
  ASSERT_TRUE(ReadAllBody(&demux, h_head, &b_head));
  EXPECT_TRUE(b_head.empty());
  HttpDestroyRequestInternal(h_head);

  // 5. 307 with streamed body fails cleanly (T4).
  const std::string body_307 = "cannot-replay";
  int64_t h_307 = HttpCreateRequestInternal(
      "POST", server_a.Url("/body-307").c_str(), "", "",
      static_cast<int64_t>(body_307.size()));
  HttpStartRequestInternal(h_307);
  ASSERT_TRUE(WriteAllBody(&demux, h_307,
                           reinterpret_cast<const uint8_t*>(body_307.data()),
                           body_307.size()));
  EXPECT_EQ(WaitHeaders(&demux, h_307), -1);
  EXPECT_NE(HttpGetErrorInternal(h_307).find("Cannot replay"),
            std::string::npos);
  HttpDestroyRequestInternal(h_307);

  // 6. Redirect target containing a space is percent-encoded (C3).
  int64_t h_sp = HttpCreateRequestInternal(
      "GET", server_a.Url("/space-redir").c_str(), "", "", kNoRequestContent);
  HttpStartRequestInternal(h_sp);
  ASSERT_EQ(WaitHeaders(&demux, h_sp), 200);
  std::string b_sp;
  ASSERT_TRUE(ReadAllBody(&demux, h_sp, &b_sp));
  EXPECT_EQ(b_sp, "space");
  HttpDestroyRequestInternal(h_sp);

  {
    std::lock_guard<std::mutex> lock(obs_mu);
    EXPECT_EQ(observed_301_method, "GET");
    EXPECT_EQ(observed_303_post_method, "GET");
    EXPECT_EQ(observed_303_head_method, "HEAD");
    EXPECT_EQ(observed_space_target, "/path%20with%20space");
  }

  // 7. Infinite redirect loop fails once kMaxRedirects is exceeded.
  int64_t h3 = HttpCreateRequestInternal("GET", server_a.Url("/loop").c_str(),
                                         "", "", kNoRequestContent);
  HttpStartRequestInternal(h3);
  EXPECT_EQ(WaitHeaders(&demux, h3), -1);
  EXPECT_NE(HttpGetErrorInternal(h3).find("Maximum HTTP redirects"),
            std::string::npos);
  HttpDestroyRequestInternal(h3);

  // 8. Non-HTTP(S) redirect target is blocked.
  int64_t h4 = HttpCreateRequestInternal("GET", server_a.Url("/unsafe").c_str(),
                                         "", "", kNoRequestContent);
  HttpStartRequestInternal(h4);
  EXPECT_EQ(WaitHeaders(&demux, h4), -1);
  EXPECT_NE(HttpGetErrorInternal(h4).find("Blocked unsafe"), std::string::npos);
  HttpDestroyRequestInternal(h4);
}

TEST(HttpTransportCurlTest, BadProxyAndInvalidHeadersFailWithoutConnecting) {
  BlockingEventDemux demux;
  demux.DrainStale();

  std::atomic<int> hits{0};
  LoopbackHttpServer server([&](const ParsedRequest&, SocketFd fd) {
    hits.fetch_add(1);
    SendAll(fd, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
  });

  // Bad proxy URL (socks5://) must fail, never connect direct (T4).
  int64_t h1 = HttpCreateRequestInternal("GET", server.Url("/p").c_str(), "",
                                         "socks5://127.0.0.1:1080",
                                         kNoRequestContent);
  HttpStartRequestInternal(h1);
  EXPECT_EQ(WaitHeaders(&demux, h1), -1);
  EXPECT_EQ(HttpGetErrorClassInternal(h1), kHttpErrorOther);
  HttpDestroyRequestInternal(h1);

  // Header containing CR injection must fail without connecting (S2, T4).
  int64_t h2 = HttpCreateRequestInternal(
      "GET", server.Url("/h").c_str(), "X-Test\tvalue\rInjected: 1\n", "",
      kNoRequestContent);
  HttpStartRequestInternal(h2);
  EXPECT_EQ(WaitHeaders(&demux, h2), -1);
  EXPECT_EQ(HttpGetErrorClassInternal(h2), kHttpErrorOther);
  HttpDestroyRequestInternal(h2);

  // Header containing LF injection (line without separator) must fail (S2, T4).
  int64_t h3 = HttpCreateRequestInternal(
      "GET", server.Url("/h2").c_str(), "X-Test\tvalue\nmalformed_line\n", "",
      kNoRequestContent);
  HttpStartRequestInternal(h3);
  EXPECT_EQ(WaitHeaders(&demux, h3), -1);
  EXPECT_EQ(HttpGetErrorClassInternal(h3), kHttpErrorOther);
  HttpDestroyRequestInternal(h3);

  EXPECT_EQ(hits.load(), 0);
}

TEST(HttpTransportCurlTest, ErrorClassesConnectAndTls) {
  BlockingEventDemux demux;
  demux.DrainStale();

  // 1. Connect error against a closed loopback port.
  EnsureSocketSubsystemInitialized();
  SocketFd probe = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_NE(probe, kInvalidSocket);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  ASSERT_EQ(bind(probe, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
  sockaddr_in bound{};
#if defined(_WIN32)
  int blen = sizeof(bound);
#else
  socklen_t blen = sizeof(bound);
#endif
  ASSERT_EQ(getsockname(probe, reinterpret_cast<sockaddr*>(&bound), &blen), 0);
  int closed_port = ntohs(bound.sin_port);
  CloseSocket(probe);

  std::string closed_url =
      "http://127.0.0.1:" + std::to_string(closed_port) + "/refused";
  int64_t h_conn = HttpCreateRequestInternal("GET", closed_url.c_str(), "", "",
                                             kNoRequestContent);
  HttpStartRequestInternal(h_conn);
  EXPECT_EQ(WaitHeaders(&demux, h_conn), -1);
  EXPECT_EQ(HttpGetErrorClassInternal(h_conn), kHttpErrorConnect);
  HttpDestroyRequestInternal(h_conn);

  // 2. TLS handshake failure when speaking HTTPS to a non-TLS endpoint.
  SocketFd tls_listen = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_NE(tls_listen, kInvalidSocket);
  int reuse = 1;
  setsockopt(tls_listen, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&reuse), sizeof(reuse));
  sockaddr_in taddr{};
  taddr.sin_family = AF_INET;
  taddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  taddr.sin_port = 0;
  ASSERT_EQ(bind(tls_listen, reinterpret_cast<sockaddr*>(&taddr), sizeof(taddr)),
            0);
  ASSERT_EQ(listen(tls_listen, 4), 0);
  sockaddr_in tbound{};
#if defined(_WIN32)
  int tblen = sizeof(tbound);
#else
  socklen_t tblen = sizeof(tbound);
#endif
  ASSERT_EQ(
      getsockname(tls_listen, reinterpret_cast<sockaddr*>(&tbound), &tblen), 0);
  int tls_port = ntohs(tbound.sin_port);

  std::atomic<bool> stop_tls_server{false};
  std::thread bad_tls_server([tls_listen, &stop_tls_server]() {
    while (!stop_tls_server.load(std::memory_order_acquire)) {
      if (!WaitForSocketReadable(tls_listen, 50)) {
        continue;
      }
      sockaddr_in cli{};
#if defined(_WIN32)
      int clen = sizeof(cli);
#else
      socklen_t clen = sizeof(cli);
#endif
      SocketFd c =
          accept(tls_listen, reinterpret_cast<sockaddr*>(&cli), &clen);
      if (c == kInvalidSocket) {
        break;
      }
      char buf[512];
      (void)recv(c, buf, sizeof(buf), 0);
      // Respond with a TLS 1.2 fatal handshake_failure alert so every TLS stack
      // (OpenSSL/BoringSSL, SecureTransport/Network.framework, Conscrypt)
      // classifies the failure as kHttpErrorTls even if it retries once.
      const char kTlsAlert[] = {0x15, 0x03, 0x03, 0x00, 0x02, 0x02, 0x28};
      SendAll(c, kTlsAlert, sizeof(kTlsAlert));
      ShutdownWrite(c);
      CloseSocket(c);
    }
    CloseSocket(tls_listen);
  });

  std::string https_url =
      "https://127.0.0.1:" + std::to_string(tls_port) + "/bad-tls";
  int64_t h_tls = HttpCreateRequestInternal("GET", https_url.c_str(), "", "",
                                            kNoRequestContent);
  HttpStartRequestInternal(h_tls);
  EXPECT_EQ(WaitHeaders(&demux, h_tls), -1);
  EXPECT_EQ(HttpGetErrorClassInternal(h_tls), kHttpErrorTls);
  HttpDestroyRequestInternal(h_tls);
  stop_tls_server.store(true, std::memory_order_release);
  bad_tls_server.join();
}

TEST(HttpTransportCurlTest,
     StreamingChunkedUploadAndLargeDownloadBackpressure) {
  BlockingEventDemux demux;
  demux.DrainStale();

  constexpr size_t kUploadSize = 1024 * 1024;        // 1 MiB (> 256 KiB pipe)
  constexpr size_t kDownloadSize = 4 * 1024 * 1024;  // 4 MiB (> 2 MiB pipe)

  std::vector<uint8_t> expected_upload(kUploadSize);
  for (size_t i = 0; i < kUploadSize; ++i) {
    expected_upload[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);
  }
  std::vector<uint8_t> expected_download(kDownloadSize);
  for (size_t i = 0; i < kDownloadSize; ++i) {
    expected_download[i] = static_cast<uint8_t>((i * 17 + 3) & 0xFF);
  }

  std::mutex upload_mu;
  std::string server_received_upload;
  LoopbackHttpServer server([&](const ParsedRequest& req, SocketFd fd) {
    if (req.target == "/upload") {
      {
        std::lock_guard<std::mutex> lock(upload_mu);
        server_received_upload = req.body;
      }
      SendAll(fd,
              "HTTP/1.1 200 OK\r\n"
              "Content-Length: 2\r\n"
              "Connection: close\r\n\r\nok");
    } else if (req.target == "/download") {
      SendAll(fd,
              "HTTP/1.1 200 OK\r\n"
              "Content-Length: " +
                  std::to_string(expected_download.size()) +
                  "\r\nConnection: close\r\n\r\n");
      SendAll(fd, reinterpret_cast<const char*>(expected_download.data()),
              expected_download.size());
    }
  });

  // 1. Unknown-length chunked streaming upload.
  int64_t h_up = HttpCreateRequestInternal(
      "PUT", server.Url("/upload").c_str(),
      "Content-Type\tapplication/octet-stream\n", "", kChunkedRequestContent);
  HttpStartRequestInternal(h_up);
  ASSERT_TRUE(
      WriteAllBody(&demux, h_up, expected_upload.data(), expected_upload.size()));
  ASSERT_EQ(WaitHeaders(&demux, h_up), 200);
  std::string up_resp;
  ASSERT_TRUE(ReadAllBody(&demux, h_up, &up_resp));
  EXPECT_EQ(up_resp, "ok");
  {
    std::lock_guard<std::mutex> lock(upload_mu);
    ASSERT_EQ(server_received_upload.size(), expected_upload.size());
    EXPECT_EQ(std::memcmp(server_received_upload.data(), expected_upload.data(),
                          expected_upload.size()),
              0);
  }
  HttpDestroyRequestInternal(h_up);

  // 2. Large download exceeding kMaxResponseBufferBytes: wait until the
  // native buffer actually crosses kMaxResponseBufferBytes (explicit condition
  // instead of a fixed sleep, T3) to deterministically exercise pause/resume.
  int64_t h_dl = HttpCreateRequestInternal(
      "GET", server.Url("/download").c_str(), "", "", kNoRequestContent);
  HttpStartRequestInternal(h_dl);
  ASSERT_EQ(WaitHeaders(&demux, h_dl), 200);

  auto dl_op = FindOperation(h_dl);
  ASSERT_NE(dl_op, nullptr);
  auto hw_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  bool hit_high_water = false;
  while (std::chrono::steady_clock::now() < hw_deadline) {
    {
      std::lock_guard<std::mutex> lock(dl_op->mutex);
      if (dl_op->resp_buffered_bytes >= kMaxResponseBufferBytes ||
          dl_op->resp_eof) {
        hit_high_water = dl_op->resp_buffered_bytes >= kMaxResponseBufferBytes;
        break;
      }
    }
    std::this_thread::yield();
  }
  EXPECT_TRUE(hit_high_water);

  std::string dl_body;
  dl_body.reserve(kDownloadSize);
  ASSERT_TRUE(ReadAllBody(&demux, h_dl, &dl_body, 15000));
  ASSERT_EQ(dl_body.size(), expected_download.size());
  EXPECT_EQ(std::memcmp(dl_body.data(), expected_download.data(),
                        expected_download.size()),
            0);
  HttpDestroyRequestInternal(h_dl);
}

TEST(HttpTransportCurlTest, ConcurrentMixedTransfers) {
  BlockingEventDemux demux;
  demux.DrainStale();

  LoopbackHttpServer server([](const ParsedRequest& req, SocketFd fd) {
    if (req.target.rfind("/echo/", 0) == 0) {
      std::string payload(64 * 1024, req.target.back());
      SendAll(fd,
              "HTTP/1.1 200 OK\r\n"
              "Content-Length: " +
                  std::to_string(payload.size()) +
                  "\r\nConnection: close\r\n\r\n" + payload);
    } else if (req.target.rfind("/sse/", 0) == 0) {
      SendAll(fd,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/event-stream\r\n"
              "Transfer-Encoding: chunked\r\n"
              "Connection: close\r\n\r\n");
      for (int i = 0; i < 4; ++i) {
        std::string evt =
            "data: " + req.target + "#" + std::to_string(i) + "\n\n";
        char hex[32];
        std::snprintf(hex, sizeof(hex), "%zx\r\n", evt.size());
        SendAll(fd, hex);
        SendAll(fd, evt);
        SendAll(fd, "\r\n");
      }
      SendAll(fd, "0\r\n\r\n");
    }
  });

  constexpr int kOps = 6;
  std::vector<int64_t> handles;
  handles.reserve(kOps);
  for (int i = 0; i < 3; ++i) {
    char tag = static_cast<char>('A' + i);
    std::string url = server.Url(std::string("/echo/") + tag);
    int64_t h = HttpCreateRequestInternal("GET", url.c_str(), "", "",
                                          kNoRequestContent);
    HttpStartRequestInternal(h);
    handles.push_back(h);
  }
  for (int i = 0; i < 3; ++i) {
    std::string url = server.Url("/sse/" + std::to_string(i));
    int64_t h = HttpCreateRequestInternal("GET", url.c_str(),
                                          "Accept\ttext/event-stream\n", "",
                                          kNoRequestContent);
    HttpStartRequestInternal(h);
    handles.push_back(h);
  }

  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(WaitHeaders(&demux, handles[i]), 200);
    std::string body;
    ASSERT_TRUE(ReadAllBody(&demux, handles[i], &body));
    EXPECT_EQ(body.size(), 64u * 1024u);
    EXPECT_EQ(body.front(), static_cast<char>('A' + i));
    EXPECT_EQ(body.back(), static_cast<char>('A' + i));
    HttpDestroyRequestInternal(handles[i]);
  }
  for (int i = 3; i < 6; ++i) {
    ASSERT_EQ(WaitHeaders(&demux, handles[i]), 200);
    std::string body;
    ASSERT_TRUE(ReadAllBody(&demux, handles[i], &body));
    std::string expected_last =
        "data: /sse/" + std::to_string(i - 3) + "#3\n\n";
    EXPECT_NE(body.find(expected_last), std::string::npos);
    HttpDestroyRequestInternal(handles[i]);
  }
}

TEST(HttpTransportCurlTest, EarlyServerResponseUnblocksLargeUpload) {
  BlockingEventDemux demux;
  demux.DrainStale();

  // Server sends an immediate 413 response with Content-Length: 0 as soon as
  // request headers arrive, then performs an RFC 9112 section 9.6 lingering
  // close (shutdown write + drain until peer closes) so the client reliably
  // receives the 413 rather than racing a BSD TCP RST from unread upload bytes.
  EnsureSocketSubsystemInitialized();
  SocketFd listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_NE(listen_fd, kInvalidSocket);
  int reuse = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&reuse), sizeof(reuse));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  ASSERT_EQ(bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)),
            0);
  ASSERT_EQ(listen(listen_fd, 4), 0);
  sockaddr_in bound{};
#if defined(_WIN32)
  int bound_len = sizeof(bound);
#else
  socklen_t bound_len = sizeof(bound);
#endif
  ASSERT_EQ(
      getsockname(listen_fd, reinterpret_cast<sockaddr*>(&bound), &bound_len),
      0);
  int raw_port = ntohs(bound.sin_port);

  std::thread raw_server([listen_fd]() {
    sockaddr_in client_addr{};
#if defined(_WIN32)
    int len = sizeof(client_addr);
#else
    socklen_t len = sizeof(client_addr);
#endif
    SocketFd c =
        accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &len);
    if (c != kInvalidSocket) {
#ifdef SO_NOSIGPIPE
      int nosigpipe = 1;
      setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE,
                 reinterpret_cast<const char*>(&nosigpipe), sizeof(nosigpipe));
#endif
      std::string hdrs;
      char tmp[1024];
      while (hdrs.find("\r\n\r\n") == std::string::npos) {
#if defined(_WIN32)
        int n = recv(c, tmp, sizeof(tmp), 0);
#else
        ssize_t n = recv(c, tmp, sizeof(tmp), 0);
#endif
        if (n <= 0) break;
        hdrs.append(tmp, static_cast<size_t>(n));
      }
      SendAll(c,
              "HTTP/1.1 413 Payload Too Large\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n"
              "\r\n");
      ShutdownWrite(c);
      (void)WaitForPeerClose(c, 1000);
      CloseSocket(c);
    }
    CloseSocket(listen_fd);
  });

  constexpr size_t kLargeUpload = 2 * 1024 * 1024;
  std::string url =
      "http://127.0.0.1:" + std::to_string(raw_port) + "/early-reject";
  int64_t h = HttpCreateRequestInternal(
      "PUT", url.c_str(), "Content-Type\tapplication/octet-stream\n", "",
      static_cast<int64_t>(kLargeUpload));
  HttpStartRequestInternal(h);

  std::vector<uint8_t> payload(kLargeUpload, 0x42);
  auto t0 = std::chrono::steady_clock::now();
  bool finished_upload =
      WriteAllBody(&demux, h, payload.data(), payload.size(), 5000);
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
  EXPECT_FALSE(finished_upload);
  EXPECT_LT(elapsed_ms, 3000);
  EXPECT_EQ(WaitHeaders(&demux, h, 2000), 413);

  HttpDestroyRequestInternal(h);
  raw_server.join();
}

}  // namespace
}  // namespace http
}  // namespace firebase
