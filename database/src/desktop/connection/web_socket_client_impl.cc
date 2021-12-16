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

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/thread.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

WebSocketClientImpl::WebSocketClientImpl(
    const std::string& uri, const std::string& user_agent, Logger* logger,
    scheduler::Scheduler* scheduler,
    WebSocketClientEventHandler* handler /*=nullptr*/)
    : uri_(uri),
      handler_(handler),
      thread_(nullptr),
      hub_(),
      keep_loop_alive_(nullptr),
      process_queue_async_(nullptr),
      callback_queue_(),
      callback_queue_mutex_(Mutex::kModeNonRecursive),
      is_destructing_(0),
      websocket_(nullptr),
      user_agent_(user_agent),
      logger_(logger),
      scheduler_(scheduler),
      safe_this_(this) {
  // Bind callback function
  hub_.onError(WebSocketClientImpl::OnError);
  hub_.onConnection(WebSocketClientImpl::OnConnection);
  hub_.onMessage(WebSocketClientImpl::OnMessage);
  hub_.onDisconnection(WebSocketClientImpl::OnDisconnection);

  // Create a async object to keep the loop alive and close all async handler
  // during destruction.
  keep_loop_alive_ = new uS::Async(hub_.getLoop());
  keep_loop_alive_->setData(this);
  keep_loop_alive_->start([](uS::Async* async) {
    assert(async);
    assert(async->getData());

    WebSocketClientImpl* client =
        static_cast<WebSocketClientImpl*>(async->getData());

    // Close all async process
    if (client) {
      client->CloseSync();

      client->keep_loop_alive_->close();
      client->keep_loop_alive_ = nullptr;
      client->process_queue_async_->close();
      client->process_queue_async_ = nullptr;
    }
  });

  // Initiate async handler to process callback queue.  The callback will only
  // be triggered after process_queue_async_->send().
  process_queue_async_ = new uS::Async(hub_.getLoop());
  process_queue_async_->setData(this);
  process_queue_async_->start(ProcessCallbackQueue);

  // Start the event loop
  thread_ = MakeUnique<Thread>(EventLoopRoutine, this);
}

void WebSocketClientImpl::EventLoopRoutine(void* data) {
  assert(data != nullptr);
  WebSocketClientImpl* client = static_cast<WebSocketClientImpl*>(data);
  Logger* logger = client->logger_;

  logger->LogDebug("=== uWebSockets Event Loop Start ===");
  client->hub_.run();
  logger->LogDebug("=== uWebSockets Event Loop End ===");
}

WebSocketClientImpl::~WebSocketClientImpl() {
  // Clear safe reference immediately so that scheduled callback can skip
  // executing code which requires reference to this.
  safe_this_.ClearReference();

  is_destructing_.store(1);

  // Remove the handler to keep event loop alive
  if (keep_loop_alive_ != nullptr) {
    keep_loop_alive_->send();
  }

  // Wait for the thread to end.
  if (thread_) {
    thread_->Join();
    thread_.reset(nullptr);
  }

  handler_ = nullptr;

  // websocket_ should be cleared now or OnDisconnection() probably is not
  // called properly.
  assert(websocket_ == nullptr);
}

void WebSocketClientImpl::Connect(int timeout_ms) {
  ScheduleOnce(
      [](WebSocketClientImpl* client, int timeout_ms, const std::string&) {
        Logger* logger = client->logger_;
        if (client->websocket_ == nullptr) {
          std::map<std::string, std::string> headers;
          headers["User-Agent"] = client->user_agent_;
          headers[app_common::kApiClientHeader] = App::GetUserAgent();
          client->hub_.connect(client->uri_, client, headers, timeout_ms);
        } else {
          logger->LogWarning("websocket has already been connected to %s",
                             client->uri_.c_str());
        }
      },
      timeout_ms, "");
}

void WebSocketClientImpl::Close() {
  ScheduleOnce([](WebSocketClientImpl* client, int,
                  const std::string&) { client->CloseSync(); },
               0, "");
}

void WebSocketClientImpl::CloseSync() {
  if (IsWebSocketAvailable()) {
    websocket_->close(1000);
  }
}

void WebSocketClientImpl::Send(const char* msg) {
  assert(msg != nullptr);

  ScheduleOnce(
      [](WebSocketClientImpl* client, int, const std::string& msg) {
        Logger* logger = client->logger_;
        if (client->IsWebSocketAvailable()) {
          client->websocket_->send(msg.c_str());
        } else {
          logger->LogWarning(
              "Cannot send message.  websocket is not available");
        }
      },
      0, msg);
}

void WebSocketClientImpl::OnError(void* data) {
  assert(data != nullptr);
  WebSocketClientImpl* client = static_cast<WebSocketClientImpl*>(data);
  Logger* logger = client->logger_;

  if (client->handler_) {
    // TODO(b/71873743): Modify uWebSockets to provide more context, ex. reasons
    WebSocketClientErrorData error(client->uri_.c_str());
    client->handler_->OnError(error);
  }

  logger->LogDebug("Error occurred while establishing connection to %s",
                   client->uri_.c_str());
}

void WebSocketClientImpl::OnConnection(ClientWebSocket* ws,
                                       uWS::HttpRequest req) {
  assert(ws != nullptr);
  assert(ws->getUserData() != nullptr);
  WebSocketClientImpl* client =
      static_cast<WebSocketClientImpl*>(ws->getUserData());

  // There should be only one connection per client.  However, hub_ can have
  // multiple connnections at a time, and current implementation does not
  // prevent Connect() from  being called when another connection is
  // establishing. Use assert for now to prevent this from happening.
  assert(client->websocket_ == nullptr);
  client->websocket_ = ws;

  if (client->handler_) {
    client->scheduler_->Schedule(new callback::CallbackValue1<ClientRef>(
        client->safe_this_, [](ClientRef client_ref) {
          ClientRefLock lock(&client_ref);
          auto client = lock.GetReference();
          if (client != nullptr && client->handler_ != nullptr) {
            client->handler_->OnOpen();
          }
        }));
  }

  // Just in case of the client is delete right after connection request is sent
  // This may have racing condition with the destructor.  But since the
  // destructor schedule another task, which runs on the same thread to this
  // call, in order to close the websocket, it should not be a big issue.
  if (client->is_destructing_.load() > 0) {
    client->CloseSync();
  }
}

void WebSocketClientImpl::OnMessage(ClientWebSocket* ws, char* message,
                                    size_t length, uWS::OpCode opCode) {
  assert(ws != nullptr);
  assert(ws->getUserData() != nullptr);
  WebSocketClientImpl* client =
      static_cast<WebSocketClientImpl*>(ws->getUserData());

  if (client->handler_) {
    std::string message_string(message, length);
    client->scheduler_->Schedule(
        new callback::CallbackValue2<ClientRef, std::string>(
            client->safe_this_, message_string,
            [](ClientRef client_ref, std::string msg) {
              ClientRefLock lock(&client_ref);
              auto client = lock.GetReference();
              if (client != nullptr && client->handler_ != nullptr) {
                client->handler_->OnMessage(msg.c_str());
              }
            }));
  }
}

void WebSocketClientImpl::OnDisconnection(ClientWebSocket* ws, int code,
                                          char* message, size_t length) {
  assert(ws != nullptr);
  assert(ws->getUserData() != nullptr);
  WebSocketClientImpl* client =
      static_cast<WebSocketClientImpl*>(ws->getUserData());

  assert(client->websocket_ != nullptr);
  client->websocket_ = nullptr;

  if (client->handler_) {
    client->scheduler_->Schedule(new callback::CallbackValue1<ClientRef>(
        client->safe_this_, [](ClientRef client_ref) {
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

  MutexLock lock(callback_queue_mutex_);
  callback_queue_.push(CallbackData(cb, this, int_value, string_value));

  // Signal the event loop to trigger the async callback.
  process_queue_async_->send();
}

void WebSocketClientImpl::ProcessCallbackQueue(uS::Async* async) {
  assert(async);
  assert(async->getData());

  WebSocketClientImpl* client =
      static_cast<WebSocketClientImpl*>(async->getData());

  MutexLock lock(client->callback_queue_mutex_);
  while (!client->callback_queue_.empty()) {
    auto& callback_data = client->callback_queue_.front();
    callback_data.callback(callback_data.client, callback_data.int_value,
                           callback_data.string_value);
    client->callback_queue_.pop();
  }
}

bool WebSocketClientImpl::IsWebSocketAvailable() const {
  return websocket_ != nullptr && !websocket_->isClosed() &&
         !websocket_->isShuttingDown();
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
