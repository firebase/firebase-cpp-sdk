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
#include "app/src/semaphore.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

// Simple WebSocket based Echo Server using third_party/uWebSockets
// It has some quirk. Ex. hub_ needs a handler (async_) to wake the loop before
// closing it or the event loop will never stop.
class TestWebSocketEchoServer {
 public:
  explicit TestWebSocketEchoServer(int port)
      : port_(port), run_(false), thread_(nullptr), keep_alive_(nullptr) {
    hub_.onMessage([](uWS::WebSocket<uWS::SERVER>* ws, char* message,
                      size_t length, uWS::OpCode opCode) {
      // Echo back immediately
      ws->send(message, length, opCode);
    });
    hub_.onConnection(
        [](uWS::WebSocket<uWS::SERVER>* ws, uWS::HttpRequest request) {
          LogDebug("[Server] Received connection from (%s) %s port: %d",
                   ws->getAddress().family, ws->getAddress().address,
                   ws->getAddress().port);
        });
    hub_.onDisconnection([](uWS::WebSocket<uWS::SERVER>* ws, int code,
                            char* message, size_t length) {
      LogDebug("[Server] Disconnected from (%s) %s port: %d",
               ws->getAddress().family, ws->getAddress().address,
               ws->getAddress().port);
    });
  }

  ~TestWebSocketEchoServer() { Stop(); }

  void Start() {
    keep_alive_ = new uS::Async(hub_.getLoop());
    keep_alive_->setData(this);
    keep_alive_->start([](uS::Async* async) {
      TestWebSocketEchoServer* server =
          static_cast<TestWebSocketEchoServer*>(async->getData());
      assert(server != nullptr);
      // close ths group in event loop thread
      server->hub_.getDefaultGroup<uWS::SERVER>().close();
      async->close();
    });

    run_ = true;
    thread_ = new std::thread([this]() {
      auto listen = [&](int port){
        if (hub_.listen(port)) {
          LogDebug("[Server] Starts to listen to port %d", port);
          return true;
        } else {
          LogDebug("[Server] Cannot listen to port %d", port);
          return false;
        }
      };

      if (port_ == 0) {
        int attempts = 1000;
        int port = 0;
        bool res = false;

        do {
          --attempts;
          port = 10000 + (rand() % 55000); // NOLINT
          res = listen(port);
        } while (run_ == true && res == false && attempts != 0);

        if (res) {
          port_ = port;
          hub_.run();  // Blocks until done
        } else if (attempts == 0) {
          LogError("Failed to find free port after 1000 attempts");
        }
      } else {
        if (listen(port_) == true) {
          hub_.run();  // Blocks until done
        } else {
          LogWarning("[Server] Cannot listen to port %d", port_.load());
        }
      }

      run_ = false;
    });
  }

  void Stop() {
    run_ = false;

    if (keep_alive_) {
      keep_alive_->send();
      keep_alive_ = nullptr;
    }

    if (thread_ != nullptr) {
      thread_->join();
      delete thread_;
      thread_ = nullptr;
    }
  }

  int GetPort(bool waitForPort = false) const {
    while (waitForPort == true && run_ == true && port_ == 0) {
      firebase::internal::Sleep(10);
    }

    return port_;
  }

 private:
  std::atomic<int> port_;
  std::atomic<bool> run_;  // Is the listen thread started and running
  uWS::Hub hub_;
  std::thread* thread_;
  uS::Async* keep_alive_;
};

std::string GetLocalHostUri(int port) {
  std::stringstream ss;
  ss << "ws://localhost:" << port;
  return ss.str();
}

class TestClientEventHandler : public WebSocketClientEventHandler {
 public:
  explicit TestClientEventHandler(Semaphore* s)
      : is_connected_(false),
        is_msg_received_(false),
        msg_received_(),
        is_closed_(false),
        is_error_(false),
        semaphore_(s) {}
  ~TestClientEventHandler() override{};

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
    semaphore_->Post();
  }

  bool is_connected_ = false;
  bool is_msg_received_ = false;
  std::string msg_received_;
  bool is_closed_ = false;
  bool is_error_ = false;

 private:
  Semaphore* semaphore_;
};

// Test if the client can connect to a local echo server, send a message,
// receive message and close the connection properly.
TEST(WebSocketClientImpl, Test1) {
  // Launch a local echo server
  TestWebSocketEchoServer server(0);
  server.Start();

  auto uri = GetLocalHostUri(server.GetPort(true));

  Semaphore semaphore(1);
  TestClientEventHandler handler(&semaphore);
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client(uri.c_str(), "", &logger, &scheduler, &handler);

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

  // Stop the server
  server.Stop();
}

// Test if it is safe to create the client and destroy it immediately.
// This is to test if the destructor can properly end the event loop.
// Otherwise, it would block forever and timeout
TEST(WebSocketClientImpl, TestEdgeCase1) {
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;
  WebSocketClientImpl ws_client("ws://localhost", "", &logger, &scheduler);
}

// Test if it is safe to connect to a server and destroy the client immediately.
// This is to test if the destructor can properly end the event loop
// Otherwise, it would block forever and timeout
TEST(WebSocketClientImpl, TestEdgeCase2) {
  // Launch a local echo server
  TestWebSocketEchoServer server(0);
  server.Start();
  Logger logger(nullptr);
  scheduler::Scheduler scheduler;

  auto uri = GetLocalHostUri(server.GetPort(true));

  int count = 0;
  while ((count++) < 10000) {
    WebSocketClientImpl* ws_client =
        new WebSocketClientImpl(uri.c_str(), "", &logger, &scheduler);

    // Connect to local server
    LogDebug("[Client][%d] Connecting to %s", count, uri.c_str());
    ws_client->Connect(5000);

    // Immediately destroy the client right after connect request
    delete ws_client;
  }

  // Stop the server
  server.Stop();
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
