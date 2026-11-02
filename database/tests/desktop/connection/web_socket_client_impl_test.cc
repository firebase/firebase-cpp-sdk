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
#include <chrono>  // NOLINT
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>  // NOLINT
#include <sstream>
#include <string>
#include <thread>  // NOLINT
#include <vector>

#include "app/rest/curl/curl_io_loop.h"
#include "app/rest/request.h"
#include "app/rest/response.h"
#include "app/rest/tests/test_http_server.h"
#include "app/rest/transport_curl.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/semaphore.h"
#include "app/src/time.h"
#include "database/src/desktop/util_desktop.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#if defined(__linux__)
#include <dirent.h>
#endif

#if FIREBASE_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
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

#if FIREBASE_PLATFORM_WINDOWS
typedef SOCKET SocketHandle;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void CloseSocket(SocketHandle s) { closesocket(s); }
#else
typedef int SocketHandle;
constexpr SocketHandle kInvalidSocket = -1;
void CloseSocket(SocketHandle s) { close(s); }
#endif

// RFC 6455 magic GUID concatenated with Sec-WebSocket-Key before SHA-1 hashing.
constexpr char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Self-contained RFC 6455 localhost WebSocket test server.
class TestWebSocketEchoServer {
 public:
  enum ServerBehavior {
    kBehaviorEcho = 0,
    kBehaviorSendFragmentedWithInterleavedPing,
    kBehaviorServerInitiatedClose,
    // Writes the 101 response and a text frame in one send() so both arrive
    // in the same TCP segment.
    kBehaviorSendTextWithHandshake,
    // Sends a PING right after the handshake and then stops reading for a
    // while so a large client message cannot be flushed in one go.
    kBehaviorPingThenDelayedRead,
    // Waits before answering the handshake so clients can be torn down while
    // the upgrade is still in flight.
    kBehaviorDelayedHandshake,
  };

  static const char* kTextWithHandshakePayload() { return "hello-with-101"; }
  static const char* kDelayedReadPingPayload() { return "late-ping"; }

  explicit TestWebSocketEchoServer(int port = 0,
                                   ServerBehavior behavior = kBehaviorEcho)
      : port_(port),
        behavior_(behavior),
        run_(false),
        listen_sock_(kInvalidSocket),
        thread_(nullptr),
        pong_received_(false) {
#if FIREBASE_PLATFORM_WINDOWS
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
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
    if (listen_sock_ != kInvalidSocket) {
      CloseSocket(listen_sock_);
      listen_sock_ = kInvalidSocket;
    }
    if (thread_ != nullptr) {
      thread_->join();
      delete thread_;
      thread_ = nullptr;
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

  static bool RecvExact(SocketHandle sock, void* buf, size_t len) {
    char* ptr = static_cast<char*>(buf);
    while (len > 0) {
      auto r = recv(sock, ptr, static_cast<int>(len), 0);
      if (r <= 0) {
        return false;
      }
      ptr += r;
      len -= static_cast<size_t>(r);
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

  // Sleeps in slices so Stop() stays prompt.
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
    // Set a short socket receive timeout so aborted client connections during
    // TestEdgeCase2 do not block the server thread.
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

    std::string accept_key;
    GetBase64SHA1(ws_key + kWebSocketGuid, &accept_key);

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " +
        accept_key + "\r\n\r\n";
    if (behavior_ == kBehaviorDelayedHandshake) {
      SleepWhileRunning(500);
      if (!run_.load()) return;
    }
    if (behavior_ == kBehaviorSendTextWithHandshake) {
      // Append an unmasked, unfragmented text frame to the handshake bytes.
      const char* payload = kTextWithHandshakePayload();
      response.push_back(static_cast<char>(0x81));
      response.push_back(static_cast<char>(std::strlen(payload)));
      response.append(payload);
    }
    if (!SendAll(sock, response.data(), response.size())) {
      return;
    }

    if (behavior_ == kBehaviorPingThenDelayedRead) {
      const char* payload = kDelayedReadPingPayload();
      SendFrame(sock, 0x89, payload, std::strlen(payload));
      // Leave the client's outbound data unread so its socket buffer fills.
      SleepWhileRunning(1000);
      if (!run_.load()) return;
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

  Semaphore semaphore(1);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client(uri.c_str(), "test-agent/1.0", &logger,
                                &scheduler, "", &handler);

  // Connect to local server
  LogDebug("[Client] Connecting to %s", uri.c_str());
  EXPECT_TRUE(semaphore.TryWait());
  ws_client.Connect(5000);
  semaphore.Wait();
  semaphore.Post();
  EXPECT_TRUE(handler.is_connected_ && !handler.is_error_);

  // Send a message and wait for the response
  EXPECT_TRUE(semaphore.TryWait());
  ws_client.Send("Hello World");
  semaphore.Wait();
  semaphore.Post();
  EXPECT_TRUE(handler.is_msg_received_ && !handler.is_error_);
  EXPECT_STREQ("Hello World", handler.msg_received_.c_str());

  // Close the connection
  EXPECT_TRUE(semaphore.TryWait());
  ws_client.Close();
  semaphore.Wait();
  semaphore.Post();
  EXPECT_TRUE(handler.is_closed_ && !handler.is_error_);

  server.Stop();
}

// Test if it is safe to create the client and destroy it immediately.
// This is to test if the destructor can properly end the event loop.
// Otherwise, it would block forever and timeout.
TEST(WebSocketClientImpl, TestEdgeCase1) {
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client("ws://127.0.0.1", "", &logger, &scheduler, "");
}

// Test if it is safe to connect to a server and destroy the client immediately.
// This is to test if the destructor can properly end the event loop.
// Otherwise, it would block forever and timeout.
TEST(WebSocketClientImpl, TestEdgeCase2) {
  TestWebSocketEchoServer server(0);
  server.Start();
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;

  auto uri = GetLocalHostUri(server.GetPort(true));

  for (int count = 0; count < 200; ++count) {
    WebSocketClientImpl* ws_client =
        new WebSocketClientImpl(uri.c_str(), "", &logger, &scheduler, "");
    ws_client->Connect(5000);
    delete ws_client;
  }

  server.Stop();
}

// Test large messages (>64KB, requiring multiple curl_ws_recv chunks) and
// multi-frame fragmented messages (CURLWS_CONT) with an interleaved PING.
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

  // Send a large 100KB payload to exercise multi-chunk curl_ws_recv reassembly
  // and flush the client's PONG back to the server.
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

  // Verify HTTP Upgrade request headers included User-Agent, X-Firebase-Client,
  // and the refreshed X-Firebase-AppCheck token.
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

// ---------------------------------------------------------------------------
// Shared CurlIoLoop tests.
// ---------------------------------------------------------------------------

namespace {

// Holds a transport reference for the duration of a test so the shared loop
// stays alive between clients and can be inspected.
class SharedLoopScope {
 public:
  SharedLoopScope() { firebase::rest::InitTransportCurl(); }
  ~SharedLoopScope() { firebase::rest::CleanupTransportCurl(); }
  firebase::rest::curl::CurlIoLoop& loop() {
    return firebase::rest::curl::CurlIoLoop::Get();
  }
  // Runs fn on the I/O thread and waits for it.
  void RunOnLoop(std::function<void()> fn) {
    Semaphore done(0);
    loop().Post([&]() {
      fn();
      done.Post();
    });
    ASSERT_TRUE(done.TimedWait(5000));
  }
  size_t transfer_count() {
    size_t count = 0;
    RunOnLoop([&]() { count = loop().transfer_count(); });
    return count;
  }
  size_t extra_fd_count() {
    size_t count = 0;
    RunOnLoop([&]() { count = loop().extra_fd_count(); });
    return count;
  }
  std::thread::id io_thread_id() {
    std::thread::id id;
    RunOnLoop([&]() { id = std::this_thread::get_id(); });
    return id;
  }
};

int64_t ElapsedMs(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start)
      .count();
}

#if defined(__linux__)
int CountThreadsInProcess() {
  int count = 0;
  DIR* dir = opendir("/proc/self/task");
  if (dir == nullptr) return -1;
  while (readdir(dir) != nullptr) count++;
  closedir(dir);
  return count - 2;  // "." and ".."
}
#endif  // defined(__linux__)

// A Response that signals a semaphore on completion.
class SignalingResponse : public firebase::rest::Response {
 public:
  explicit SignalingResponse(Semaphore* semaphore) : semaphore_(semaphore) {}
  void MarkCompleted() override {
    firebase::rest::Response::MarkCompleted();
    semaphore_->Post();
  }
  void MarkFailed() override {
    firebase::rest::Response::MarkFailed();
    semaphore_->Post();
  }

 private:
  Semaphore* semaphore_;
};

}  // namespace

// Two clients to two servers share one I/O thread; closing one leaves the
// other fully functional.
TEST(WebSocketClientImpl, TwoClientsShareOneLoop) {
  SharedLoopScope scope;
  TestWebSocketEchoServer server_a(0);
  TestWebSocketEchoServer server_b(0);
  server_a.Start();
  server_b.Start();

  Semaphore semaphore_a(0);
  Semaphore semaphore_b(0);
  TestClientEventHandler handler_a(&semaphore_a);
  TestClientEventHandler handler_b(&semaphore_b);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl client_a(GetLocalHostUri(server_a.GetPort(true)), "a",
                               &logger, &scheduler, "", &handler_a);
  WebSocketClientImpl client_b(GetLocalHostUri(server_b.GetPort(true)), "b",
                               &logger, &scheduler, "", &handler_b);

  std::thread::id io_thread = scope.io_thread_id();
  EXPECT_NE(std::this_thread::get_id(), io_thread);

  client_a.Connect(5000);
  client_b.Connect(5000);
  EXPECT_TRUE(semaphore_a.TimedWait(5000));
  EXPECT_TRUE(semaphore_b.TimedWait(5000));
  EXPECT_TRUE(handler_a.is_connected_);
  EXPECT_TRUE(handler_b.is_connected_);

  // Both open sockets are serviced by the same loop...
  EXPECT_EQ(2u, scope.extra_fd_count());
  EXPECT_EQ(0u, scope.transfer_count());
  // ...on the one and only I/O thread.
  EXPECT_EQ(io_thread, scope.io_thread_id());

  client_a.Send("from-a");
  client_b.Send("from-b");
  EXPECT_TRUE(semaphore_a.TimedWait(5000));
  EXPECT_TRUE(semaphore_b.TimedWait(5000));
  EXPECT_EQ("from-a", handler_a.msg_received_);
  EXPECT_EQ("from-b", handler_b.msg_received_);

  client_a.Close();
  EXPECT_TRUE(semaphore_a.TimedWait(5000));
  EXPECT_TRUE(handler_a.is_closed_);
  EXPECT_EQ(1u, scope.extra_fd_count());

  // B is unaffected.
  handler_b.is_msg_received_ = false;
  client_b.Send("still-alive");
  EXPECT_TRUE(semaphore_b.TimedWait(5000));
  EXPECT_TRUE(handler_b.is_msg_received_);
  EXPECT_EQ("still-alive", handler_b.msg_received_);
  EXPECT_FALSE(handler_b.is_closed_);
  EXPECT_FALSE(handler_a.is_error_);
  EXPECT_FALSE(handler_b.is_error_);

  client_b.Close();
  EXPECT_TRUE(semaphore_b.TimedWait(5000));
  EXPECT_TRUE(handler_b.is_closed_);
  EXPECT_EQ(0u, scope.extra_fd_count());

  server_a.Stop();
  server_b.Stop();
}

// A slow REST transfer on the shared multi handle must not delay WebSocket
// traffic, and an open WebSocket must not delay REST traffic.
TEST(WebSocketClientImpl,
     SharesLoopWithRestTransportWithoutHeadOfLineBlocking) {
  SharedLoopScope scope;
  TestWebSocketEchoServer ws_server(0);
  ws_server.Start();
  firebase::rest::test::TestHttpServer http_server;
  ASSERT_TRUE(http_server.Start());

  Semaphore ws_semaphore(0);
  TestClientEventHandler handler(&ws_semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl client(GetLocalHostUri(ws_server.GetPort(true)), "",
                             &logger, &scheduler, "", &handler);
  client.Connect(5000);
  EXPECT_TRUE(ws_semaphore.TimedWait(5000));
  ASSERT_TRUE(handler.is_connected_);

  // Start a REST request that stays in flight for 2 s.
  Semaphore slow_done(0);
  firebase::rest::Request slow_request;
  slow_request.set_url((http_server.BaseUrl() + "/slow?ms=2000").c_str());
  SignalingResponse slow_response(&slow_done);
  firebase::rest::TransportCurl slow_transport;
  slow_transport.set_is_async(true);
  slow_transport.Perform(slow_request, &slow_response);
  EXPECT_EQ(1u, scope.transfer_count());

  // WebSocket round trips are not held up by it.
  for (int i = 0; i < 5; ++i) {
    handler.is_msg_received_ = false;
    auto start = std::chrono::steady_clock::now();
    client.Send("ping-while-rest-busy");
    ASSERT_TRUE(ws_semaphore.TimedWait(5000));
    EXPECT_LT(ElapsedMs(start), 200) << "round trip " << i;
    EXPECT_EQ("ping-while-rest-busy", handler.msg_received_);
  }

  // Nor is a quick REST request held up by the open WebSocket.
  {
    Semaphore fast_done(0);
    firebase::rest::Request fast_request;
    fast_request.set_url((http_server.BaseUrl() + "/").c_str());
    SignalingResponse fast_response(&fast_done);
    firebase::rest::TransportCurl fast_transport;
    auto start = std::chrono::steady_clock::now();
    fast_transport.Perform(fast_request, &fast_response);  // synchronous
    EXPECT_LT(ElapsedMs(start), 200);
    EXPECT_EQ(200, fast_response.status());
    EXPECT_STREQ("test", fast_response.GetBody());
  }

  // The slow one still completes normally.
  EXPECT_TRUE(slow_done.TimedWait(10000));
  EXPECT_EQ(200, slow_response.status());

  client.Close();
  EXPECT_TRUE(ws_semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_closed_);
  EXPECT_FALSE(handler.is_error_);
  ws_server.Stop();
}

// A data frame that arrives in the same TCP segment as the 101 response must
// be delivered.
TEST(WebSocketClientImpl, FrameInSameSegmentAsHandshakeIsDelivered) {
  TestWebSocketEchoServer server(
      0, TestWebSocketEchoServer::kBehaviorSendTextWithHandshake);
  server.Start();

  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl client(GetLocalHostUri(server.GetPort(true)), "", &logger,
                             &scheduler, "", &handler);
  client.Connect(5000);
  // OnOpen followed by OnMessage.
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_connected_);
  EXPECT_TRUE(handler.is_msg_received_);
  EXPECT_EQ(TestWebSocketEchoServer::kTextWithHandshakePayload(),
            handler.msg_received_);
  EXPECT_FALSE(handler.is_error_);

  client.Close();
  EXPECT_TRUE(semaphore.TimedWait(5000));
  server.Stop();
}

// Destroying clients while their handshake is in flight leaves nothing behind
// on the shared loop.
TEST(WebSocketClientImpl, DestroyDuringHandshakeLeavesSharedLoopClean) {
  SharedLoopScope scope;
  TestWebSocketEchoServer server(
      0, TestWebSocketEchoServer::kBehaviorDelayedHandshake);
  server.Start();
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  auto uri = GetLocalHostUri(server.GetPort(true));

  for (int i = 0; i < 20; ++i) {
    Semaphore semaphore(0);
    TestClientEventHandler handler(&semaphore);
    WebSocketClientImpl* client =
        new WebSocketClientImpl(uri, "", &logger, &scheduler, "", &handler);
    client->Connect(5000);
    if (i % 2 == 1) {
      // Let the handshake actually get registered before tearing down.
      EXPECT_EQ(1u, scope.transfer_count());
    }
    delete client;
    EXPECT_FALSE(handler.is_connected_);
  }
  EXPECT_EQ(0u, scope.transfer_count());
  EXPECT_EQ(0u, scope.extra_fd_count());

  // The loop is still perfectly usable afterwards.
  TestWebSocketEchoServer echo_server(0);
  echo_server.Start();
  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  WebSocketClientImpl client(GetLocalHostUri(echo_server.GetPort(true)), "",
                             &logger, &scheduler, "", &handler);
  client.Connect(5000);
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_TRUE(handler.is_connected_);
  client.Send("after-churn");
  EXPECT_TRUE(semaphore.TimedWait(5000));
  EXPECT_EQ("after-churn", handler.msg_received_);

  server.Stop();
  echo_server.Stop();
}

// A PING that arrives while a large outbound frame is only partially written
// (POLLOUT armed) is answered once that frame has been flushed.
TEST(WebSocketClientImpl, PingDuringPartialSendStillGetsPong) {
  TestWebSocketEchoServer server(
      0, TestWebSocketEchoServer::kBehaviorPingThenDelayedRead);
  server.Start();

  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl client(GetLocalHostUri(server.GetPort(true)), "", &logger,
                             &scheduler, "", &handler);
  client.Connect(5000);
  EXPECT_TRUE(semaphore.TimedWait(5000));
  ASSERT_TRUE(handler.is_connected_);

  // Far more than any loopback socket buffer: the server is not reading for
  // a second, so curl_ws_send() has to stop part-way and wait for POLLOUT
  // while the server's PING is already in our receive path.
  std::string large_payload(24 * 1024 * 1024, 'z');
  client.Send(large_payload.c_str());
  EXPECT_TRUE(semaphore.TimedWait(30000));
  EXPECT_TRUE(handler.is_msg_received_);
  EXPECT_EQ(large_payload.size(), handler.msg_received_.size());
  EXPECT_EQ(large_payload, handler.msg_received_);

  // The PONG was sent after the frame completed; give the server a moment to
  // read it.
  for (int i = 0; i < 500 && !server.pong_received(); ++i) {
    firebase::internal::Sleep(10);
  }
  EXPECT_TRUE(server.pong_received());
  EXPECT_EQ(TestWebSocketEchoServer::kDelayedReadPingPayload(),
            server.last_pong_payload());
  EXPECT_FALSE(handler.is_error_);

  client.Close();
  EXPECT_TRUE(semaphore.TimedWait(5000));
  server.Stop();
}

#if defined(__linux__)
// A WebSocket client no longer brings its own thread: with the shared loop
// already running, creating, connecting and using a client creates none.
TEST(WebSocketClientImpl, ClientDoesNotCreateAThread) {
  SharedLoopScope scope;
  TestWebSocketEchoServer server(0);
  server.Start();
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  Semaphore semaphore(0);
  TestClientEventHandler handler(&semaphore);
  // The scheduler starts its worker thread lazily; warm it up so the baseline
  // below already includes it.
  scheduler.Schedule([&semaphore]() { semaphore.Post(); });
  ASSERT_TRUE(semaphore.TimedWait(5000));

  int threads_before = CountThreadsInProcess();
  ASSERT_GT(threads_before, 0);
  {
    WebSocketClientImpl client(GetLocalHostUri(server.GetPort(true)), "",
                               &logger, &scheduler, "", &handler);
    client.Connect(5000);
    EXPECT_TRUE(semaphore.TimedWait(5000));
    client.Send("hi");
    EXPECT_TRUE(semaphore.TimedWait(5000));
    EXPECT_EQ(threads_before, CountThreadsInProcess());
    client.Close();
    EXPECT_TRUE(semaphore.TimedWait(5000));
  }
  EXPECT_EQ(threads_before, CountThreadsInProcess());
  server.Stop();
}
#endif  // defined(__linux__)

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
