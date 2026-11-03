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

// A tiny, dependency-free, blocking HTTP/1.1 server bound to 127.0.0.1 for
// exercising the libcurl transport in unit tests. It follows the same
// POSIX / Winsock portability pattern as TestWebSocketEchoServer in
// database/tests/desktop/connection/web_socket_client_impl_test.cc.
//
// Behavior (every response carries "Server: TestHttpServer" and
// "Connection: close"):
//   GET  <any path>          -> 200, body "test"
//   POST <any path>          -> 200, body = request body (Content-Length or
//                               chunked request bodies are both accepted)
//   <other method> <path>    -> 200, body = the method name
//   /slow?ms=N               -> like the above but sleeps N ms (default 2000)
//                               before replying
//   /large?size=N            -> 200, body of N bytes ('x')
//   /status/NNN              -> status NNN with an empty body
//
// The server records every request it has fully read so tests can assert on
// what the client actually sent.

#ifndef FIREBASE_APP_REST_TESTS_TEST_HTTP_SERVER_H_
#define FIREBASE_APP_REST_TESTS_TEST_HTTP_SERVER_H_

#include <algorithm>
#include <atomic>
#include <chrono>  // NOLINT
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>  // NOLINT
#include <string>
#include <thread>  // NOLINT
#include <vector>

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace firebase {
namespace rest {
namespace test {

#if FIREBASE_PLATFORM_WINDOWS
using SocketHandle = SOCKET;
const SocketHandle kInvalidSocket = INVALID_SOCKET;
inline void CloseSocket(SocketHandle s) { closesocket(s); }
inline bool LastSocketErrorWasTimeout() {
  int err = WSAGetLastError();
  return err == WSAETIMEDOUT || err == WSAEWOULDBLOCK;
}
#else
using SocketHandle = int;
const SocketHandle kInvalidSocket = -1;
inline void CloseSocket(SocketHandle s) { close(s); }
inline bool LastSocketErrorWasTimeout() {
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}
#endif

// Process-wide socket subsystem initialization (WSAStartup on Windows, SIGPIPE
// suppression elsewhere). Safe to construct many times.
class SocketEnvironment {
 public:
  SocketEnvironment() {
#if FIREBASE_PLATFORM_WINDOWS
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
  }
  ~SocketEnvironment() {
#if FIREBASE_PLATFORM_WINDOWS
    WSACleanup();
#endif
  }
  SocketEnvironment(const SocketEnvironment&) = delete;
  SocketEnvironment& operator=(const SocketEnvironment&) = delete;
};

inline void SetSocketRecvTimeout(SocketHandle sock, int timeout_ms) {
#if FIREBASE_PLATFORM_WINDOWS
  DWORD timeout = static_cast<DWORD>(timeout_ms);
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
             reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
  timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

inline bool SendAll(SocketHandle sock, const void* data, size_t len) {
  const char* ptr = static_cast<const char*>(data);
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
  while (len > 0) {
    const int chunk = static_cast<int>(
        std::min(len, static_cast<size_t>(std::numeric_limits<int>::max())));
    auto sent = send(sock, ptr, chunk, flags);
    if (sent < 0) {
#if !FIREBASE_PLATFORM_WINDOWS
      if (errno == EINTR) continue;
#endif
      return false;
    }
    if (sent == 0) return false;
    ptr += sent;
    len -= static_cast<size_t>(sent);
  }
  return true;
}

// A pair of connected loopback TCP sockets, for tests that need a real file
// descriptor to wait on.
class TcpSocketPair {
 public:
  TcpSocketPair() : socket_environment_(), a_(kInvalidSocket), b_(kInvalidSocket) {
    SocketHandle listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidSocket) return;
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(listener, 1) != 0) {
      CloseSocket(listener);
      return;
    }
    socklen_t addr_len = sizeof(addr);
    getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &addr_len);
    a_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (a_ != kInvalidSocket &&
        connect(a_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
      b_ = accept(listener, nullptr, nullptr);
    }
    CloseSocket(listener);
    if (b_ == kInvalidSocket && a_ != kInvalidSocket) {
      CloseSocket(a_);
      a_ = kInvalidSocket;
    }
  }
  ~TcpSocketPair() {
    if (a_ != kInvalidSocket) CloseSocket(a_);
    if (b_ != kInvalidSocket) CloseSocket(b_);
  }
  TcpSocketPair(const TcpSocketPair&) = delete;
  TcpSocketPair& operator=(const TcpSocketPair&) = delete;

  bool valid() const { return a_ != kInvalidSocket && b_ != kInvalidSocket; }
  SocketHandle a() const { return a_; }
  SocketHandle b() const { return b_; }

 private:
  SocketEnvironment socket_environment_;
  SocketHandle a_;
  SocketHandle b_;
};

struct RecordedRequest {
  std::string method;
  std::string path;
  std::map<std::string, std::string> headers;  // lower-cased names
  std::string body;
};

class TestHttpServer {
 public:
  static const char* kServerName() { return "TestHttpServer"; }

  TestHttpServer()
      : port_(0),
        run_(false),
        listen_sock_(kInvalidSocket),
        accept_thread_(),
        request_count_(0) {}

  ~TestHttpServer() { Stop(); }

  TestHttpServer(const TestHttpServer&) = delete;
  TestHttpServer& operator=(const TestHttpServer&) = delete;

  // Binds, listens and starts accepting. Returns false on failure.
  bool Start() {
    listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock_ == kInvalidSocket) return false;

    int opt = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) !=
            0 ||
        listen(listen_sock_, 128) != 0) {
      CloseSocket(listen_sock_);
      listen_sock_ = kInvalidSocket;
      return false;
    }
    sockaddr_in bound_addr;
    socklen_t addr_len = sizeof(bound_addr);
    if (getsockname(listen_sock_, reinterpret_cast<sockaddr*>(&bound_addr),
                    &addr_len) != 0) {
      CloseSocket(listen_sock_);
      listen_sock_ = kInvalidSocket;
      return false;
    }
    port_.store(ntohs(bound_addr.sin_port));
    run_.store(true);
    accept_thread_ = std::thread([this]() { RunAcceptLoop(); });
    return true;
  }

  void Stop() {
    run_.store(false);
    if (accept_thread_.joinable()) {
      accept_thread_.join();
    }
    if (listen_sock_ != kInvalidSocket) {
      CloseSocket(listen_sock_);
      listen_sock_ = kInvalidSocket;
    }
    std::vector<std::thread> workers;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      workers.swap(workers_);
    }
    for (std::thread& worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  int port() const { return port_.load(); }

  std::string BaseUrl() const {
    return "http://127.0.0.1:" + std::to_string(port());
  }

  int request_count() const { return request_count_.load(); }

  // Copy of the most recently *completed* request, or an empty one.
  RecordedRequest last_request() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_request_;
  }

 private:
  static bool RecvSome(SocketHandle sock, std::string* out,
                       const std::atomic<bool>& run) {
    char buffer[4096];
    for (;;) {
      auto r = recv(sock, buffer, sizeof(buffer), 0);
      if (r > 0) {
        out->append(buffer, static_cast<size_t>(r));
        return true;
      }
      if (r == 0) return false;  // Peer closed.
      if (LastSocketErrorWasTimeout() && run.load()) continue;
      return false;
    }
  }

  static std::string ToLower(std::string s) {
    for (char& c : s) {
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
  }

  static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
  }

  // Parses "name=value" query parameters out of path. Returns path sans query.
  static std::string SplitQuery(const std::string& target,
                                std::map<std::string, std::string>* query) {
    size_t q = target.find('?');
    if (q == std::string::npos) return target;
    std::string rest = target.substr(q + 1);
    size_t pos = 0;
    while (pos <= rest.size()) {
      size_t amp = rest.find('&', pos);
      std::string pair = rest.substr(
          pos, amp == std::string::npos ? std::string::npos : amp - pos);
      size_t eq = pair.find('=');
      if (eq != std::string::npos) {
        (*query)[pair.substr(0, eq)] = pair.substr(eq + 1);
      } else if (!pair.empty()) {
        (*query)[pair] = "";
      }
      if (amp == std::string::npos) break;
      pos = amp + 1;
    }
    return target.substr(0, q);
  }

  // Reads one request (head + body). Returns false if the connection was
  // closed or timed out before a complete request arrived.
  bool ReadRequest(SocketHandle sock, RecordedRequest* request) {
    std::string data;
    size_t head_end;
    while ((head_end = data.find("\r\n\r\n")) == std::string::npos) {
      if (data.size() > 64 * 1024) return false;
      if (!RecvSome(sock, &data, run_)) return false;
    }
    std::string head = data.substr(0, head_end);
    std::string body = data.substr(head_end + 4);

    // Request line.
    size_t line_end = head.find("\r\n");
    std::string request_line = head.substr(0, line_end);
    size_t sp1 = request_line.find(' ');
    size_t sp2 = request_line.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return false;
    request->method = request_line.substr(0, sp1);
    request->path = request_line.substr(sp1 + 1, sp2 - sp1 - 1);

    // Headers.
    size_t pos = line_end == std::string::npos ? head.size() : line_end + 2;
    while (pos < head.size()) {
      size_t next = head.find("\r\n", pos);
      std::string line = head.substr(
          pos, next == std::string::npos ? std::string::npos : next - pos);
      size_t colon = line.find(':');
      if (colon != std::string::npos) {
        request->headers[ToLower(Trim(line.substr(0, colon)))] =
            Trim(line.substr(colon + 1));
      }
      if (next == std::string::npos) break;
      pos = next + 2;
    }

    // libcurl sends "Expect: 100-continue" for uploads of unknown length and
    // waits up to a second for the interim response before sending the body.
    auto expect = request->headers.find("expect");
    if (expect != request->headers.end() &&
        ToLower(expect->second) == "100-continue") {
      static const char kContinue[] = "HTTP/1.1 100 Continue\r\n\r\n";
      SendAll(sock, kContinue, sizeof(kContinue) - 1);
    }

    // Body.
    auto te = request->headers.find("transfer-encoding");
    auto cl = request->headers.find("content-length");
    if (te != request->headers.end() &&
        ToLower(te->second).find("chunked") != std::string::npos) {
      // Chunked: read until the terminating zero-length chunk.
      std::string decoded;
      for (;;) {
        size_t crlf;
        while ((crlf = body.find("\r\n")) == std::string::npos) {
          if (!RecvSome(sock, &body, run_)) return false;
        }
        size_t chunk_size = static_cast<size_t>(
            std::strtoul(body.substr(0, crlf).c_str(), nullptr, 16));
        body.erase(0, crlf + 2);
        if (chunk_size == 0) {
          // Consume the trailer terminator if present.
          while (body.find("\r\n") == std::string::npos) {
            if (!RecvSome(sock, &body, run_)) break;
          }
          break;
        }
        if (chunk_size > std::numeric_limits<size_t>::max() - 2) {
          return false;
        }
        while (body.size() < chunk_size + 2) {
          if (!RecvSome(sock, &body, run_)) return false;
        }
        decoded.append(body, 0, chunk_size);
        body.erase(0, chunk_size + 2);
      }
      request->body = decoded;
    } else if (cl != request->headers.end()) {
      size_t length =
          static_cast<size_t>(std::strtoul(cl->second.c_str(), nullptr, 10));
      while (body.size() < length) {
        if (!RecvSome(sock, &body, run_)) return false;
      }
      request->body = body.substr(0, length);
    } else {
      request->body.clear();
    }
    return true;
  }

  // Sleeps in slices so Stop() stays prompt.
  void SleepWhileRunning(int ms) {
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (run_.load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  void HandleClient(SocketHandle sock) {
    SetSocketRecvTimeout(sock, 200);
    RecordedRequest request;
    if (!ReadRequest(sock, &request)) return;

    std::map<std::string, std::string> query;
    std::string path = SplitQuery(request.path, &query);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      last_request_ = request;
    }
    request_count_.fetch_add(1);

    int status = 200;
    std::string body;
    if (path == "/slow") {
      int ms = 2000;
      auto it = query.find("ms");
      if (it != query.end()) ms = std::atoi(it->second.c_str());
      SleepWhileRunning(ms);
      if (!run_.load()) return;
    }
    if (path == "/large") {
      size_t size = 1024 * 1024;
      auto it = query.find("size");
      if (it != query.end()) {
        size =
            static_cast<size_t>(std::strtoul(it->second.c_str(), nullptr, 10));
      }
      body.assign(size, 'x');
    } else if (path.compare(0, 8, "/status/") == 0) {
      status = std::atoi(path.c_str() + 8);
    } else if (request.method == "GET") {
      body = "test";
    } else if (request.method == "POST") {
      body = request.body;
    } else {
      body = request.method;
    }

    std::string response = "HTTP/1.1 " + std::to_string(status) + " " +
                           ReasonPhrase(status) +
                           "\r\n"
                           "Server: " +
                           std::string(kServerName()) +
                           "\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: " +
                           std::to_string(body.size()) +
                           "\r\n"
                           "Connection: close\r\n"
                           "\r\n";
    response += body;
    SendAll(sock, response.data(), response.size());
  }

  static const char* ReasonPhrase(int status) {
    switch (status) {
      case 200:
        return "OK";
      case 204:
        return "No Content";
      case 400:
        return "Bad Request";
      case 404:
        return "Not Found";
      case 500:
        return "Internal Server Error";
      default:
        return "Status";
    }
  }

  void RunAcceptLoop() {
    while (run_.load()) {
      SocketHandle lsock = listen_sock_;
      if (lsock == kInvalidSocket) break;
#if !FIREBASE_PLATFORM_WINDOWS
      if (lsock < 0 || lsock >= FD_SETSIZE) break;
#endif
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(lsock, &read_fds);
      timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 50 * 1000;  // 50ms
      int sel =
          select(static_cast<int>(lsock + 1), &read_fds, nullptr, nullptr, &tv);
      if (sel <= 0 || !run_.load()) continue;

      sockaddr_in client_addr;
      socklen_t client_len = sizeof(client_addr);
      SocketHandle client_sock =
          accept(lsock, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
      if (client_sock == kInvalidSocket) continue;

      // One thread per connection so a /slow request never blocks others.
      std::lock_guard<std::mutex> lock(mutex_);
      workers_.emplace_back([this, client_sock]() {
        HandleClient(client_sock);
        CloseSocket(client_sock);
      });
    }
  }

  SocketEnvironment socket_environment_;
  std::atomic<int> port_;
  std::atomic<bool> run_;
  SocketHandle listen_sock_;
  std::thread accept_thread_;
  mutable std::mutex mutex_;
  std::vector<std::thread> workers_;
  RecordedRequest last_request_;
  std::atomic<int> request_count_;
};

}  // namespace test
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_TESTS_TEST_HTTP_SERVER_H_
