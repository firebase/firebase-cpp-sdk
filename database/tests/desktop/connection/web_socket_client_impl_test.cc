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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "app/src/include/firebase/internal/platform.h"
#include "app/src/semaphore.h"
#include "app/src/time.h"
#include "database/src/desktop/util_desktop.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

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
namespace database {
namespace internal {
namespace connection {

namespace {

// Platform socket compatibility helpers:
// - Windows (Winsock2) represents sockets as unsigned `SOCKET` handles, uses
//   `INVALID_SOCKET` as the error sentinel, closes sockets via `closesocket()`,
//   and reports socket errors via `WSAGetLastError()`.
// - POSIX (Linux/macOS) represents sockets as signed `int` file descriptors,
//   uses `-1` as the error sentinel, closes sockets via `close()`, and reports
//   socket errors via `errno`.
#if FIREBASE_PLATFORM_WINDOWS
typedef SOCKET SocketHandle;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void CloseSocket(SocketHandle s) { closesocket(s); }
bool LastSocketErrorWasTimeout() {
  int err = WSAGetLastError();
  return err == WSAETIMEDOUT || err == WSAEWOULDBLOCK;
}
#else
typedef int SocketHandle;
constexpr SocketHandle kInvalidSocket = -1;
void CloseSocket(SocketHandle s) { close(s); }
bool LastSocketErrorWasTimeout() {
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}
#endif

// RFC 6455 section 4.2.2 magic GUID concatenated with the client's
// `Sec-WebSocket-Key` header before SHA-1 hashing and Base64 encoding to form
// `Sec-WebSocket-Accept`.
constexpr char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

int64_t ElapsedMs(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start)
      .count();
}

// Self-contained RFC 6455 localhost WebSocket test server.
class TestWebSocketEchoServer {
 public:
  enum ServerBehavior {
    // Echoes client data frames back, replies to PING with PONG, and echoes
    // CLOSE before disconnecting.
    kBehaviorEcho = 0,
    // Immediately after the 101 handshake, sends a 3-fragment text message
    // with a PING control frame interleaved between fragments 1 and 2, then
    // enters the normal echo loop.
    kBehaviorSendFragmentedWithInterleavedPing,
    // Immediately after the 101 handshake, sends a normal CLOSE frame (1000)
    // and closes the connection.
    kBehaviorServerInitiatedClose,
    // Accepts the TCP connection and reads the HTTP Upgrade request but never
    // sends the 101 response, exercising the client's handshake deadline
    // watchdog.
    kBehaviorNeverAnswerHandshake,
  };

  explicit TestWebSocketEchoServer(int port = 0,
                                   ServerBehavior behavior = kBehaviorEcho)
      : port_(port),
        behavior_(behavior),
        run_(false),
        listen_sock_(kInvalidSocket),
        thread_(nullptr),
        pong_received_(false) {
#if FIREBASE_PLATFORM_WINDOWS
    // Winsock2 requires explicit per-process initialization (reference-counted
    // by the OS) before any socket APIs (`socket`, `bind`, `listen`, etc.) can
    // be called.
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
    // On POSIX, calling `send()` on a socket after the peer has closed its end
    // raises `SIGPIPE`, which terminates the test process by default. Ignore
    // `SIGPIPE` so `send()` instead fails cleanly with `EPIPE`.
    signal(SIGPIPE, SIG_IGN);
#endif
  }

  ~TestWebSocketEchoServer() {
    Stop();
#if FIREBASE_PLATFORM_WINDOWS
    WSACleanup();
#endif
  }

  void Start() {
    listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(listen_sock_, kInvalidSocket);

    int opt = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Bind to 127.0.0.1 (`INADDR_LOOPBACK`). When `port_` is 0, the OS kernel
    // assigns an available ephemeral port, which we query below via
    // `getsockname()`.
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port_.load()));

    ASSERT_EQ(
        bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)),
        0);
    ASSERT_EQ(listen(listen_sock_, 128), 0);

    sockaddr_in bound_addr;
    socklen_t addr_len = sizeof(bound_addr);
    ASSERT_EQ(getsockname(listen_sock_,
                          reinterpret_cast<sockaddr*>(&bound_addr), &addr_len),
              0);
    port_.store(ntohs(bound_addr.sin_port));

    run_.store(true);
    thread_ = new std::thread([this]() { RunAcceptLoop(); });
  }

  void Stop() {
    run_.store(false);
    if (thread_ != nullptr) {
      thread_->join();
      delete thread_;
      thread_ = nullptr;
    }
    if (listen_sock_ != kInvalidSocket) {
      CloseSocket(listen_sock_);
      listen_sock_ = kInvalidSocket;
    }
  }

  int GetPort(bool waitForPort = false) const {
    while (waitForPort && run_.load() && port_.load() == 0) {
      firebase::internal::Sleep(10);
    }
    return port_.load();
  }

  std::string GetLastHandshakeRequest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_handshake_request_;
  }

  bool pong_received() const { return pong_received_.load(); }

  std::string last_pong_payload() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_pong_payload_;
  }

 private:
  static bool SendAll(SocketHandle sock, const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags |= MSG_NOSIGNAL;
#endif
    while (len > 0) {
      auto sent = send(sock, ptr, static_cast<int>(len), flags);
      if (sent <= 0) {
        return false;
      }
      ptr += sent;
      len -= static_cast<size_t>(sent);
    }
    return true;
  }

  bool RecvExact(SocketHandle sock, void* buf, size_t len) {
    char* ptr = static_cast<char*>(buf);
    while (len > 0) {
      auto r = recv(sock, ptr, static_cast<int>(len), 0);
      if (r > 0) {
        ptr += r;
        len -= static_cast<size_t>(r);
        continue;
      }
      if (r == 0) {
        return false;
      }
      if (LastSocketErrorWasTimeout() && run_.load()) {
        continue;
      }
      return false;
    }
    return true;
  }

  static bool SendFrame(SocketHandle sock, uint8_t first_byte,
                        const void* payload, size_t len) {
    uint8_t header[10];
    size_t header_len = 0;
    header[0] = first_byte;
    if (len < 126) {
      header[1] = static_cast<uint8_t>(len);
      header_len = 2;
    } else if (len <= 0xFFFF) {
      header[1] = 126;
      header[2] = static_cast<uint8_t>((len >> 8) & 0xFF);
      header[3] = static_cast<uint8_t>(len & 0xFF);
      header_len = 4;
    } else {
      header[1] = 127;
      uint64_t l64 = static_cast<uint64_t>(len);
      for (int i = 0; i < 8; ++i) {
        header[2 + i] = static_cast<uint8_t>((l64 >> ((7 - i) * 8)) & 0xFF);
      }
      header_len = 10;
    }
    if (!SendAll(sock, header, header_len)) {
      return false;
    }
    if (len > 0 && !SendAll(sock, payload, len)) {
      return false;
    }
    return true;
  }

  // Sleeps in 10ms slices so Stop() wakes the server thread promptly.
  void SleepWhileRunning(int ms) {
    for (int waited = 0; waited < ms && run_.load(); waited += 10) {
      firebase::internal::Sleep(10);
    }
  }

  void RunAcceptLoop() {
    while (run_.load()) {
      SocketHandle lsock = listen_sock_;
      if (lsock == kInvalidSocket) {
        break;
      }
#if !FIREBASE_PLATFORM_WINDOWS
      if (lsock >= FD_SETSIZE) {
        break;
      }
#endif
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(lsock, &read_fds);
      timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 50 * 1000;  // 50ms
      int sel =
          select(static_cast<int>(lsock + 1), &read_fds, nullptr, nullptr, &tv);
      if (sel <= 0 || !run_.load()) {
        continue;
      }

      sockaddr_in client_addr;
      socklen_t client_len = sizeof(client_addr);
      SocketHandle client_sock =
          accept(lsock, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
      if (client_sock == kInvalidSocket) {
        continue;
      }
      HandleClient(client_sock);
      CloseSocket(client_sock);
    }
  }

  void HandleClient(SocketHandle sock) {
    // Set a 200ms receive timeout (`SO_RCVTIMEO`) so blocking `recv()` calls
    // wake periodically to check `run_` when `Stop()` is called, and so
    // aborted client connections in `TestEdgeCase2` that close before sending
    // a complete HTTP Upgrade request do not block the server thread.
    // Windows `setsockopt(SO_RCVTIMEO)` takes a `DWORD` millisecond count,
    // whereas POSIX takes a `struct timeval`.
#if FIREBASE_PLATFORM_WINDOWS
    DWORD timeout_ms = 200;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200 * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    // Read HTTP Upgrade request headers until "\r\n\r\n".
    std::string request;
    char ch = 0;
    while (request.find("\r\n\r\n") == std::string::npos) {
      auto r = recv(sock, &ch, 1, 0);
      if (r <= 0) {
        return;
      }
      request.push_back(ch);
      if (request.size() > 8192) {
        return;
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      last_handshake_request_ = request;
    }

    // Extract Sec-WebSocket-Key header (case-insensitive).
    std::string ws_key;
    std::istringstream stream(request);
    std::string line;
    while (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      const std::string kPrefix = "Sec-WebSocket-Key:";
      if (line.size() >= kPrefix.size()) {
        bool match = true;
        for (size_t i = 0; i < kPrefix.size(); ++i) {
          char a = line[i];
          char b = kPrefix[i];
          if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
          if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
          if (a != b) {
            match = false;
            break;
          }
        }
        if (match) {
          size_t pos = kPrefix.size();
          while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
            ++pos;
          }
          ws_key = line.substr(pos);
          break;
        }
      }
    }
    if (ws_key.empty()) {
      return;
    }

    if (behavior_ == kBehaviorNeverAnswerHandshake) {
      // Hold the connection open without replying until the server stops.
      SleepWhileRunning(30000);
      return;
    }

    std::string accept_key;
    GetBase64SHA1(ws_key + kWebSocketGuid, &accept_key);

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " +
        accept_key + "\r\n\r\n";
    if (!SendAll(sock, response.data(), response.size())) {
      return;
    }

    if (behavior_ == kBehaviorServerInitiatedClose) {
      const uint8_t close_code[2] = {0x03, 0xe8};  // 1000 Normal Closure
      SendFrame(sock, 0x88, close_code, sizeof(close_code));
      return;
    }

    if (behavior_ == kBehaviorSendFragmentedWithInterleavedPing) {
      // Send a 3-fragment text message with a PING control frame interleaved
      // between fragment 1 and fragment 2:
      // Fragment 1: FIN=0, opcode=0x1 (TEXT), payload="Part1-"
      SendFrame(sock, 0x01, "Part1-", 6);
      // Interleaved PING: FIN=1, opcode=0x9 (PING), payload="ping-data"
      SendFrame(sock, 0x89, "ping-data", 9);
      // Fragment 2: FIN=0, opcode=0x0 (CONT), payload="Part2-"
      SendFrame(sock, 0x00, "Part2-", 6);
      // Fragment 3: FIN=1, opcode=0x0 (CONT), payload="Part3"
      SendFrame(sock, 0x80, "Part3", 5);
    }

    // Read client frames and echo data frames back.
    while (run_.load()) {
      uint8_t hdr[2];
      if (!RecvExact(sock, hdr, 2)) {
        break;
      }
      uint8_t opcode = hdr[0] & 0x0F;
      bool masked = (hdr[1] & 0x80) != 0;
      uint64_t payload_len = hdr[1] & 0x7F;
      if (payload_len == 126) {
        uint8_t ext[2];
        if (!RecvExact(sock, ext, 2)) break;
        payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
      } else if (payload_len == 127) {
        uint8_t ext[8];
        if (!RecvExact(sock, ext, 8)) break;
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
          payload_len = (payload_len << 8) | ext[i];
        }
      }

      uint8_t mask_key[4] = {0, 0, 0, 0};
      if (masked && !RecvExact(sock, mask_key, 4)) {
        break;
      }

      std::vector<uint8_t> payload(static_cast<size_t>(payload_len));
      if (payload_len > 0 && !RecvExact(sock, payload.data(), payload.size())) {
        break;
      }
      if (masked) {
        for (size_t i = 0; i < payload.size(); ++i) {
          payload[i] ^= mask_key[i & 3];
        }
      }

      if (opcode == 0x8) {
        // CLOSE frame: echo CLOSE back and exit.
        SendFrame(sock, 0x88, payload.data(), payload.size());
        break;
      } else if (opcode == 0x9) {
        // PING frame: reply with PONG.
        SendFrame(sock, 0x8A, payload.data(), payload.size());
      } else if (opcode == 0xA) {
        // PONG frame: record payload.
        {
          std::lock_guard<std::mutex> lock(mutex_);
          last_pong_payload_.assign(
              reinterpret_cast<const char*>(payload.data()), payload.size());
        }
        pong_received_.store(true);
      } else {
        // Echo the frame back to the client.
        if (!SendFrame(sock, hdr[0], payload.data(), payload.size())) {
          break;
        }
      }
    }
  }

  std::atomic<int> port_;
  ServerBehavior behavior_;
  std::atomic<bool> run_;
  SocketHandle listen_sock_;
  std::thread* thread_;
  mutable std::mutex mutex_;
  std::string last_handshake_request_;
  std::atomic<bool> pong_received_;
  std::string last_pong_payload_;
};

std::string GetLocalHostUri(int port) {
  std::stringstream ss;
  ss << "ws://127.0.0.1:" << port;
  return ss.str();
}

}  // namespace

class TestClientEventHandler : public WebSocketClientEventHandler {
 public:
  explicit TestClientEventHandler(Semaphore* s)
      : is_connected_(false),
        is_msg_received_(false),
        msg_received_(),
        is_closed_(false),
        is_error_(false),
        semaphore_(s) {}
  ~TestClientEventHandler() override {}

  void OnOpen() override {
    is_connected_ = true;
    semaphore_->Post();
  }

  void OnMessage(const char* msg) override {
    is_msg_received_ = true;
    msg_received_ = msg;
    semaphore_->Post();
  }

  void OnClose() override {
    is_closed_ = true;
    semaphore_->Post();
  }

  void OnError(const WebSocketClientErrorData& error_data) override {
    is_error_ = true;
    error_uri_ = error_data.GetUri();
    semaphore_->Post();
  }

  bool is_connected_ = false;
  bool is_msg_received_ = false;
  std::string msg_received_;
  bool is_closed_ = false;
  bool is_error_ = false;
  std::string error_uri_;

 private:
  Semaphore* semaphore_;
};

// Test if the client can connect to a local echo server, send a message,
// receive a message and close the connection properly.
TEST(WebSocketClientImpl, Test1) {
  TestWebSocketEchoServer server(0);
  server.Start();

  auto uri = GetLocalHostUri(server.GetPort(true));

  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client(uri.c_str(), "test-agent/1.0", &logger,
                                &scheduler, "", &handler);

  // Connect to local server.
  ws_client.Connect(5000);
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_connected_ && !handler.is_error_);

  // Send a message and wait for the echoed response.
  ws_client.Send("Hello World");
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_msg_received_ && !handler.is_error_);
  EXPECT_STREQ("Hello World", handler.msg_received_.c_str());

  // Close the connection.
  ws_client.Close();
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_closed_ && !handler.is_error_);

  server.Stop();
}

// Test if it is safe to create the client and destroy it immediately.
// This verifies that the destructor cleanly joins the I/O thread even when
// Connect() was never called.
TEST(WebSocketClientImpl, TestEdgeCase1) {
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client("ws://127.0.0.1", "", &logger, &scheduler, "");
}

// Test if it is safe to connect to a server and destroy the client while the
// handshake is in flight. Each iteration spawns and joins an I/O thread, so the
// loop is bounded by both an iteration count and wall-clock time to keep the
// test fast on slow CI machines while still exercising the race thousands of
// times on fast ones.
TEST(WebSocketClientImpl, TestEdgeCase2) {
  TestWebSocketEchoServer server(0);
  server.Start();
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;

  auto uri = GetLocalHostUri(server.GetPort(true));

  constexpr int kMinIterations = 500;
  constexpr int kMaxIterations = 10000;
  constexpr int64_t kTimeBudgetMs = 10 * 1000;
  auto start = std::chrono::steady_clock::now();
  int count = 0;
  for (; count < kMaxIterations; ++count) {
    if (count >= kMinIterations && ElapsedMs(start) >= kTimeBudgetMs) break;
    WebSocketClientImpl* ws_client =
        new WebSocketClientImpl(uri.c_str(), "", &logger, &scheduler, "");
    ws_client->Connect(5000);
    delete ws_client;
  }
  EXPECT_GE(count, kMinIterations);

  server.Stop();
}

// Destroying the client from inside one of its own scheduled handler callbacks
// is how the Realtime Database tears a connection down (Connection::OnError ->
// Close -> PersistentConnection resets the Connection, which owns the client).
// The callback lambda must keep the client's SafeReference alive and Join()
// must not deadlock while the scheduler thread is the one destroying it.
TEST(WebSocketClientImpl, DestroyClientFromOwnErrorCallback) {
  int closed_port = 0;
  {
    TestWebSocketEchoServer server(0);
    server.Start();
    closed_port = server.GetPort(true);
    server.Stop();
  }
  auto uri = GetLocalHostUri(closed_port);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;

  class DestroyingHandler : public WebSocketClientEventHandler {
   public:
    explicit DestroyingHandler(Semaphore* s) : semaphore_(s) {}
    void OnOpen() override {}
    void OnMessage(const char*) override {}
    void OnClose() override {}
    void OnError(const WebSocketClientErrorData&) override {
      client_.reset();
      semaphore_->Post();
    }
    std::unique_ptr<WebSocketClientImpl> client_;

   private:
    Semaphore* semaphore_;
  };

  for (int i = 0; i < 20; ++i) {
    Semaphore semaphore(0);
    DestroyingHandler handler(&semaphore);
    handler.client_ = std::make_unique<WebSocketClientImpl>(
        uri.c_str(), "", &logger, &scheduler, "", &handler);
    handler.client_->Connect(2000);
    EXPECT_TRUE(semaphore.TimedWait(5000));
    EXPECT_EQ(handler.client_, nullptr);
  }
}

// Test large messages (>16KB kRecvBufferSize, requiring multiple curl_ws_recv
// chunks), multi-frame fragmented messages (CURLWS_CONT) with an interleaved
// PING, and HTTP Upgrade headers (User-Agent and X-Firebase-AppCheck).
TEST(WebSocketClientImpl, LargeAndFragmentedMessagesWithInterleavedPing) {
  TestWebSocketEchoServer server(
      0, TestWebSocketEchoServer::kBehaviorSendFragmentedWithInterleavedPing);
  server.Start();

  auto uri = GetLocalHostUri(server.GetPort(true));

  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client(uri.c_str(), "custom-agent/2.0", &logger,
                                &scheduler, "initial-token", &handler);
  ws_client.RefreshAppCheckToken("refreshed-app-check-token");

  ws_client.Connect(5000);
  // Wait for OnOpen() AND the initial fragmented message sent by the server.
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_connected_);
  EXPECT_FALSE(handler.is_error_);
  EXPECT_TRUE(handler.is_msg_received_);
  EXPECT_EQ(handler.msg_received_, "Part1-Part2-Part3");

  // Send a 100KB payload to exercise multi-chunk curl_ws_recv reassembly and
  // flush the client's PONG back to the server.
  std::string large_payload(100 * 1024, 'A');
  for (size_t i = 0; i < large_payload.size(); ++i) {
    large_payload[i] = static_cast<char>('A' + (i % 26));
  }
  handler.is_msg_received_ = false;
  handler.msg_received_.clear();
  ws_client.Send(large_payload.c_str());
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_msg_received_);
  EXPECT_EQ(handler.msg_received_, large_payload);

  // Verify the server received the PONG matching its interleaved PING.
  EXPECT_TRUE(server.pong_received());
  EXPECT_EQ(server.last_pong_payload(), "ping-data");

  // Verify HTTP Upgrade request headers included User-Agent and the refreshed
  // X-Firebase-AppCheck token.
  std::string handshake = server.GetLastHandshakeRequest();
  EXPECT_NE(handshake.find("User-Agent: custom-agent/2.0\r\n"),
            std::string::npos);
  EXPECT_NE(
      handshake.find("X-Firebase-AppCheck: refreshed-app-check-token\r\n"),
      std::string::npos);

  ws_client.Close();
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_closed_);

  server.Stop();
}

// Test server-initiated CLOSE frame triggers OnClose() cleanly.
TEST(WebSocketClientImpl, ServerInitiatedClose) {
  TestWebSocketEchoServer server(
      0, TestWebSocketEchoServer::kBehaviorServerInitiatedClose);
  server.Start();

  auto uri = GetLocalHostUri(server.GetPort(true));

  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client(uri.c_str(), "", &logger, &scheduler, "",
                                &handler);

  ws_client.Connect(5000);
  // Expect OnOpen() followed by OnClose().
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_connected_);
  EXPECT_TRUE(handler.is_closed_);
  EXPECT_FALSE(handler.is_error_);

  server.Stop();
}

// Test connection failure triggers OnError().
TEST(WebSocketClientImpl, ConnectionFailureTriggersOnError) {
  // Bind and immediately stop a server to obtain an unused port.
  int closed_port = 0;
  {
    TestWebSocketEchoServer server(0);
    server.Start();
    closed_port = server.GetPort(true);
    server.Stop();
  }

  auto uri = GetLocalHostUri(closed_port);
  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client(uri.c_str(), "", &logger, &scheduler, "",
                                &handler);

  ws_client.Connect(2000);
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_error_);
  EXPECT_FALSE(handler.is_connected_);
  EXPECT_EQ(handler.error_uri_, uri);
}

// libcurl bounds only DNS/TCP/TLS for a CURLOPT_CONNECT_ONLY handle; the HTTP
// Upgrade exchange itself has no libcurl-side timeout. The client's own
// watchdog must turn a server that accepts TCP but never answers the Upgrade
// into OnError() within Connect()'s timeout.
TEST(WebSocketClientImpl, HandshakeStallTimesOutViaWatchdog) {
  TestWebSocketEchoServer server(
      0, TestWebSocketEchoServer::kBehaviorNeverAnswerHandshake);
  server.Start();

  auto uri = GetLocalHostUri(server.GetPort(true));
  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client(uri.c_str(), "", &logger, &scheduler, "",
                                &handler);

  auto start = std::chrono::steady_clock::now();
  ws_client.Connect(500);
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_error_);
  EXPECT_FALSE(handler.is_connected_);
  EXPECT_EQ(handler.error_uri_, uri);
  // Fired by the watchdog (~500 ms), not by a TCP-level failure.
  EXPECT_GE(ElapsedMs(start), 400);
  EXPECT_LT(ElapsedMs(start), 4000);
  // The server did receive the Upgrade request; it just never answered.
  EXPECT_FALSE(server.GetLastHandshakeRequest().empty());

  server.Stop();
}

// Non-ws/wss URI schemes (e.g. http://, file://) must be rejected before
// opening a connection even when linked against a multi-protocol system
// libcurl.
TEST(WebSocketClientImpl, RejectsNonWebSocketUriSchemes) {
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;

  const std::string http_uri = "http://127.0.0.1:1/";
  {
    Semaphore semaphore(0);
    TestClientEventHandler handler(&semaphore);
    WebSocketClientImpl ws_client(http_uri, "", &logger, &scheduler, "",
                                  &handler);
    ws_client.Connect(5000);
    EXPECT_TRUE(semaphore.TimedWait(5000));
    EXPECT_TRUE(handler.is_error_);
    EXPECT_FALSE(handler.is_connected_);
    EXPECT_EQ(http_uri, handler.error_uri_);
  }

  const std::string file_uri = "file:///etc/hosts";
  {
    Semaphore semaphore(0);
    TestClientEventHandler handler(&semaphore);
    WebSocketClientImpl ws_client(file_uri, "", &logger, &scheduler, "",
                                  &handler);
    ws_client.Connect(5000);
    EXPECT_TRUE(semaphore.TimedWait(5000));
    EXPECT_TRUE(handler.is_error_);
    EXPECT_FALSE(handler.is_connected_);
    EXPECT_EQ(file_uri, handler.error_uri_);
  }
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
