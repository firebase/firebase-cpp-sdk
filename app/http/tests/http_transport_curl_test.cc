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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
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
    // Wake AcceptLoop() portably across Linux, macOS, and Windows (where
    // shutdown() on an unconnected listening socket returns ENOTCONN /
    // WSAENOTCONN without waking accept()). `select()` with a 50 ms timeout in
    // AcceptLoop() also guarantees termination if self-connect is refused.
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
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(fd, &read_fds);
      timeval tv{};
      tv.tv_sec = 0;
      tv.tv_usec = 50 * 1000;
#if defined(_WIN32)
      int ready = select(0, &read_fds, nullptr, nullptr, &tv);
#else
      int ready = select(fd + 1, &read_fds, nullptr, nullptr, &tv);
#endif
      if (ready <= 0) {
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
          // Consume trailing CRLF after 0-length chunk.
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

// Synchronously drives an operation via the flat bridge API until response
// headers arrive (returning HTTP status code or -1 on error/cancel).
int32_t WaitHeaders(int64_t handle, int timeout_ms = 5000) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    int32_t status = HttpPollResponseHeadersInternal(handle);
    if (status != 0) {
      return status;
    }
    HttpWaitNextEventInternal(50);
  }
  return 0;
}

bool ReadAllBody(int64_t handle, std::string* out, int timeout_ms = 10000) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  char buf[16384];
  while (std::chrono::steady_clock::now() < deadline) {
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
    HttpWaitNextEventInternal(50);
  }
  return false;
}

bool WriteAllBody(int64_t handle, const uint8_t* data, size_t len,
                  int timeout_ms = 10000) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  size_t offset = 0;
  while (offset < len && std::chrono::steady_clock::now() < deadline) {
    int32_t to_send = static_cast<int32_t>(
        (std::min)(len - offset, static_cast<size_t>(65536)));
    int32_t n = HttpTryWriteRequestBodyInternal(
        handle, const_cast<uint8_t*>(data + offset), to_send);
    if (n < 0) return false;
    if (n > 0) {
      offset += static_cast<size_t>(n);
      continue;
    }
    HttpWaitNextEventInternal(50);
  }
  if (offset != len) return false;
  HttpFinishRequestBodyInternal(handle);
  return true;
}

TEST(HttpTransportCurlTest, GetBasic200AndMultiValueHeaders) {
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
  ASSERT_EQ(WaitHeaders(h), 200);

  std::string raw_headers = HttpGetResponseHeadersInternal(h);
  EXPECT_NE(raw_headers.find("Set-Cookie\ta=1\n"), std::string::npos);
  EXPECT_NE(raw_headers.find("Set-Cookie\tb=2\n"), std::string::npos);

  std::string body;
  ASSERT_TRUE(ReadAllBody(h, &body));
  EXPECT_EQ(body, "world");
  HttpDestroyRequestInternal(h);
}

TEST(HttpTransportCurlTest, PostJsonAndError400And204NoContent) {
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
  ASSERT_TRUE(WriteAllBody(h1, reinterpret_cast<const uint8_t*>(payload.data()),
                           payload.size()));
  ASSERT_EQ(WaitHeaders(h1), 200);
  std::string body1;
  ASSERT_TRUE(ReadAllBody(h1, &body1));
  EXPECT_EQ(body1, "{\"result\":{\"echo\":\"hi\"}}");
  HttpDestroyRequestInternal(h1);

  // 2. 400 error JSON body preserved
  int64_t h2 = HttpCreateRequestInternal("POST", server.Url("/err400").c_str(),
                                         "", "", 0);
  HttpStartRequestInternal(h2);
  ASSERT_EQ(WaitHeaders(h2), 400);
  std::string body2;
  ASSERT_TRUE(ReadAllBody(h2, &body2));
  EXPECT_EQ(body2, "{\"error\":{\"status\":\"INVALID_ARGUMENT\"}}");
  HttpDestroyRequestInternal(h2);

  // 3. 204 No Content
  int64_t h3 = HttpCreateRequestInternal(
      "DELETE", server.Url("/delete").c_str(), "", "", kNoRequestContent);
  HttpStartRequestInternal(h3);
  ASSERT_EQ(WaitHeaders(h3), 204);
  std::string body3;
  ASSERT_TRUE(ReadAllBody(h3, &body3));
  EXPECT_TRUE(body3.empty());
  HttpDestroyRequestInternal(h3);
}

TEST(HttpTransportCurlTest, SseTrickleStreamingImmediateDelivery) {
  // Deterministic barrier: server does NOT send chunk 2 until the client has
  // already received and parsed chunk 1! Any intermediate buffering would time
  // out and fail this test.
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
  ASSERT_TRUE(WriteAllBody(h, reinterpret_cast<const uint8_t*>(req_body.data()),
                           req_body.size()));
  ASSERT_EQ(WaitHeaders(h), 200);

  auto read_until = [&](std::string* accum, std::string_view needle) -> bool {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    char buf[1024];
    while (accum->find(needle) == std::string::npos &&
           std::chrono::steady_clock::now() < deadline) {
      int32_t n = HttpTryReadResponseBodyInternal(h, buf, sizeof(buf));
      if (n > 0) {
        accum->append(buf, static_cast<size_t>(n));
      } else if (n == 0 || n == kHttpReadErrorOrCanceled) {
        break;
      } else {
        HttpWaitNextEventInternal(25);
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
  ASSERT_TRUE(ReadAllBody(h, &rest));
  accum.append(rest);
  EXPECT_NE(accum.find("data: {\"seq\":3}\n\n"), std::string::npos);
  HttpDestroyRequestInternal(h);
}

TEST(HttpTransportCurlTest, SseIdleCancelUnblocksPromptly) {
  std::mutex stop_mu;
  std::condition_variable stop_cv;
  bool release_server = false;

  LoopbackHttpServer server([&](const ParsedRequest&, SocketFd fd) {
    SendAll(fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "12\r\ndata: {\"ok\":true}\n\r\n");
    std::unique_lock<std::mutex> lock(stop_mu);
    stop_cv.wait_for(lock, std::chrono::seconds(10),
                     [&]() { return release_server; });
  });

  int64_t h = HttpCreateRequestInternal("GET", server.Url("/hang").c_str(),
                                        "Accept\ttext/event-stream\n", "",
                                        kNoRequestContent);
  HttpStartRequestInternal(h);
  ASSERT_EQ(WaitHeaders(h), 200);

  // Read the first SSE chunk.
  std::string got;
  char buf[512];
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (got.empty() && std::chrono::steady_clock::now() < deadline) {
    int32_t n = HttpTryReadResponseBodyInternal(h, buf, sizeof(buf));
    if (n > 0) {
      got.append(buf, static_cast<size_t>(n));
    } else {
      HttpWaitNextEventInternal(25);
    }
  }
  EXPECT_EQ(got, "data: {\"ok\":true}\n");

  // Arm a read while server is silent, then cancel from another thread.
  ASSERT_EQ(HttpTryReadResponseBodyInternal(h, buf, sizeof(buf)),
            kHttpReadWouldBlock);
  auto t0 = std::chrono::steady_clock::now();
  std::thread canceler([h]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    HttpCancelRequestInternal(h);
  });

  int32_t rc = kHttpReadWouldBlock;
  while (rc == kHttpReadWouldBlock &&
         std::chrono::steady_clock::now() - t0 < std::chrono::seconds(3)) {
    HttpWaitNextEventInternal(50);
    rc = HttpTryReadResponseBodyInternal(h, buf, sizeof(buf));
  }
  canceler.join();

  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
  EXPECT_EQ(rc, kHttpReadErrorOrCanceled);
  EXPECT_LT(elapsed_ms, 1000);

  HttpDestroyRequestInternal(h);
  {
    std::lock_guard<std::mutex> lock(stop_mu);
    release_server = true;
  }
  stop_cv.notify_all();
}

TEST(HttpTransportCurlTest,
     RedirectsSameOriginPreservesAuthAndCrossOriginStrips) {
  std::string observed_b_auth;
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
  ASSERT_EQ(WaitHeaders(h1), 200);
  std::string b1;
  ASSERT_TRUE(ReadAllBody(h1, &b1));
  EXPECT_EQ(b1, "ok");
  {
    std::lock_guard<std::mutex> lock(obs_mu);
    EXPECT_EQ(observed_same_origin_method, "GET");
    EXPECT_EQ(observed_same_origin_auth, "Bearer secret");
  }
  HttpDestroyRequestInternal(h1);

  // 2. Cross-origin 307 strips sensitive headers, forwards safe headers.
  int64_t h2 =
      HttpCreateRequestInternal("GET", server_a.Url("/cross-307").c_str(),
                                "Authorization\tBearer secret\n"
                                "X-Firebase-AppCheck\tappcheck-tok\n"
                                "x-goog-api-key\tapi-key\n"
                                "X-Safe\tkeep-me\n",
                                "", kNoRequestContent);
  HttpStartRequestInternal(h2);
  ASSERT_EQ(WaitHeaders(h2), 200);
  std::string b2;
  ASSERT_TRUE(ReadAllBody(h2, &b2));
  EXPECT_EQ(b2, "done");
  {
    std::lock_guard<std::mutex> lock(obs_mu);
    EXPECT_EQ(observed_b_method, "GET");
    EXPECT_TRUE(observed_b_auth.empty());
    EXPECT_TRUE(observed_b_appcheck.empty());
    EXPECT_TRUE(observed_b_apikey.empty());
    EXPECT_EQ(observed_b_safe, "keep-me");
  }
  HttpDestroyRequestInternal(h2);

  // 3. Infinite redirect loop fails once kMaxRedirects is exceeded.
  int64_t h3 = HttpCreateRequestInternal("GET", server_a.Url("/loop").c_str(),
                                         "", "", kNoRequestContent);
  HttpStartRequestInternal(h3);
  EXPECT_EQ(WaitHeaders(h3), -1);
  EXPECT_NE(HttpGetErrorInternal(h3).find("Maximum HTTP redirects"),
            std::string::npos);
  HttpDestroyRequestInternal(h3);

  // 4. Non-HTTP(S) redirect target is blocked.
  int64_t h4 = HttpCreateRequestInternal("GET", server_a.Url("/unsafe").c_str(),
                                         "", "", kNoRequestContent);
  HttpStartRequestInternal(h4);
  EXPECT_EQ(WaitHeaders(h4), -1);
  EXPECT_NE(HttpGetErrorInternal(h4).find("Blocked unsafe"), std::string::npos);
  HttpDestroyRequestInternal(h4);
}

TEST(HttpTransportCurlTest,
     StreamingChunkedUploadAndLargeDownloadBackpressure) {
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
      WriteAllBody(h_up, expected_upload.data(), expected_upload.size()));
  ASSERT_EQ(WaitHeaders(h_up), 200);
  std::string up_resp;
  ASSERT_TRUE(ReadAllBody(h_up, &up_resp));
  EXPECT_EQ(up_resp, "ok");
  {
    std::lock_guard<std::mutex> lock(upload_mu);
    ASSERT_EQ(server_received_upload.size(), expected_upload.size());
    EXPECT_EQ(std::memcmp(server_received_upload.data(), expected_upload.data(),
                          expected_upload.size()),
              0);
  }
  HttpDestroyRequestInternal(h_up);

  // 2. Large download exceeding kMaxResponseBufferBytes with intentional slow
  // drain to trigger CURL_WRITEFUNC_PAUSE + low-water resume.
  int64_t h_dl = HttpCreateRequestInternal(
      "GET", server.Url("/download").c_str(), "", "", kNoRequestContent);
  HttpStartRequestInternal(h_dl);
  ASSERT_EQ(WaitHeaders(h_dl), 200);

  // Pause reading briefly so curl fills the 2 MiB buffer and hits
  // WRITEFUNC_PAUSE.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::string dl_body;
  dl_body.reserve(kDownloadSize);
  ASSERT_TRUE(ReadAllBody(h_dl, &dl_body, 15000));
  ASSERT_EQ(dl_body.size(), expected_download.size());
  EXPECT_EQ(std::memcmp(dl_body.data(), expected_download.data(),
                        expected_download.size()),
            0);
  HttpDestroyRequestInternal(h_dl);
}

TEST(HttpTransportCurlTest, ConcurrentMixedTransfers) {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
    ASSERT_EQ(WaitHeaders(handles[i]), 200);
    std::string body;
    ASSERT_TRUE(ReadAllBody(handles[i], &body));
    EXPECT_EQ(body.size(), 64u * 1024u);
    EXPECT_EQ(body.front(), static_cast<char>('A' + i));
    EXPECT_EQ(body.back(), static_cast<char>('A' + i));
    HttpDestroyRequestInternal(handles[i]);
  }
  for (int i = 3; i < 6; ++i) {
    ASSERT_EQ(WaitHeaders(handles[i]), 200);
    std::string body;
    ASSERT_TRUE(ReadAllBody(handles[i], &body));
    std::string expected_last =
        "data: /sse/" + std::to_string(i - 3) + "#3\n\n";
    EXPECT_NE(body.find(expected_last), std::string::npos);
    HttpDestroyRequestInternal(handles[i]);
  }
}

TEST(HttpTransportCurlTest, EarlyServerResponseUnblocksLargeUpload) {
  // Server sends an immediate 413 response with Content-Length: 0 and closes
  // as soon as request headers arrive, without reading the >256 KiB upload
  // body. Verifies MarkResponseComplete() wakes the blocked request-body writer
  // instead of hanging forever.
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
  // WriteAllBody should terminate promptly (returning false once 413 completes)
  // rather than blocking until its 5s timeout.
  bool finished_upload = WriteAllBody(h, payload.data(), payload.size(), 5000);
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
  EXPECT_FALSE(finished_upload);
  EXPECT_LT(elapsed_ms, 3000);
  EXPECT_EQ(WaitHeaders(h, 2000), 413);

  HttpDestroyRequestInternal(h);
  raw_server.join();
}

}  // namespace
}  // namespace http
}  // namespace firebase
