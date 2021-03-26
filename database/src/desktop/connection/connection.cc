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

#include "database/src/desktop/connection/connection.h"

#include <cstdlib>
#include <cstring>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/connection/util_connection.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

const int Connection::kKeepAliveTimeoutMs = 45 * 1000;  // 45 seconds
const int Connection::kConnectTimeoutMs = 30 * 1000;    // 30 seconds
const int Connection::kMaxFrameSize = 16384;

const char* const Connection::kRequestType = "t";
const char* const Connection::kRequestTypeData = "d";
const char* const Connection::kRequestPayload = "d";
const char* const Connection::kServerEnvelopeType = "t";
const char* const Connection::kServerDataMessage = "d";
const char* const Connection::kServerControlMessage = "c";
const char* const Connection::kServerEnvelopeData = "d";
const char* const Connection::kServerControlMessageType = "t";
const char* const Connection::kServerControlMessageShutdown = "s";
const char* const Connection::kServerControlMessageReset = "r";
const char* const Connection::kServerControlMessageHello = "h";
const char* const Connection::kServerControlMessageError = "e";
const char* const Connection::kServerControlMessageData = "d";
const char* const Connection::kServerHelloTimestamp = "ts";
const char* const Connection::kServerHelloHost = "h";
const char* const Connection::kServerHelloSessionId = "s";

compat::Atomic<uint32_t> Connection::next_log_id_(0);

Connection::Connection(scheduler::Scheduler* scheduler, const HostInfo& info,
                       const char* opt_last_session_id,
                       ConnectionEventHandler* event_handler, Logger* logger)
    : safe_this_(this),
      event_handler_(event_handler),
      scheduler_(scheduler),
      host_info_(info),
      state_(kStateNone),
      ws_connected_(false),
      client_(nullptr),
      expected_incoming_frames_(0),
      logger_(logger) {
  FIREBASE_DEV_ASSERT(scheduler);
  FIREBASE_DEV_ASSERT(event_handler);

  // Create log id like "[conn_0]" for debugging
  std::stringstream log_id_stream;
  log_id_stream << "[conn_" << next_log_id_.fetch_add(1) << "]";
  log_id_ = log_id_stream.str();

  // Create web socket client regardless of its implementation
  client_ = CreateWebSocketClient(host_info_, this, opt_last_session_id, logger,
                                  scheduler);
}

Connection::~Connection() {
  // Clear safe reference immediately so that scheduled callback can skip
  // executing code which requires reference to this.
  safe_this_.ClearReference();

  // Destroy the client so that no more event will be triggered from this point.
  client_.reset(nullptr);

  // Cancel the keep_alive_handler_.  The handle is not thread-safe, so cancel
  // in worker thread.
  scheduler_->Schedule(new callback::CallbackValue1<scheduler::RequestHandle>(
      keep_alive_handler_, [](scheduler::RequestHandle handler) {
        if (handler.IsValid() && !handler.IsCancelled()) {
          FIREBASE_DEV_ASSERT(handler.Cancel());
        }
      }));
}

void Connection::Open() {
  FIREBASE_DEV_ASSERT(client_);
  if (state_ != kStateNone) {
    logger_->LogError("%s Cannot open. Connection has be opened before",
                      log_id_.c_str());
    return;
  }

  logger_->LogDebug("%s Opening a connection", log_id_.c_str());
  state_ = kStateConnecting;
  client_->Connect(kConnectTimeoutMs);
}

void Connection::Close(DisconnectReason reason /* = kReasonOther */) {
  FIREBASE_DEV_ASSERT(client_);

  if (state_ == kStateDisconnected) {
    logger_->LogError("%s Cannot close. Connection has been closed.",
                      log_id_.c_str());
    return;
  }

  logger_->LogDebug("%s Closing connection. Reason: %d", log_id_.c_str(),
                    static_cast<int>(reason));

  state_ = kStateDisconnected;

  client_->Close();

  // Cancel the repeating callback to keep the websocket connection alive
  if (keep_alive_handler_.IsValid() && !keep_alive_handler_.IsCancelled()) {
    keep_alive_handler_.Cancel();
  }

  event_handler_->OnDisconnect(reason);
}

void Connection::Send(const Variant& message, bool is_sensitive) {
  FIREBASE_DEV_ASSERT(client_);
  FIREBASE_DEV_ASSERT(!message.is_null());

  if (state_ != kStateReady) {
    logger_->LogError("%s Tried to send on an unconnected connection",
                      log_id_.c_str());
    return;
  }

  // Wrap into Firebase wire protocol Data Message format
  Variant request = Variant::EmptyMap();
  request.map()[kRequestType] = kRequestTypeData;
  request.map()[kRequestPayload] = message;

  std::string to_send = util::VariantToJson(request);
  logger_->LogDebug("%s Sending data: %s", log_id_.c_str(),
                    is_sensitive ? "(contents hidden)" : to_send.c_str());

  // Split info frames if the length is larger than kMaxFrameSize
  int num_of_frame = to_send.length() / kMaxFrameSize + 1;
  if (num_of_frame > 1) {
    logger_->LogDebug("%s Split data into %d frames (size: %d)",
                      log_id_.c_str(), num_of_frame, to_send.length());

    // Send number of frames
    std::stringstream frame_size_str;
    frame_size_str << num_of_frame;
    client_->Send(frame_size_str.str().c_str());

    // Send individual frame
    for (int i = 0; i < to_send.length(); i += kMaxFrameSize) {
      client_->Send(to_send.substr(i, kMaxFrameSize).c_str());
    }
  } else {
    client_->Send(to_send.c_str());
  }
}

void Connection::OnOpen() {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ConnectionRefLock, lock, safe_this_);

  logger_->LogDebug("%s websocket opened", log_id_.c_str());

  FIREBASE_DEV_ASSERT(state_ == kStateConnecting);

  ws_connected_ = true;

  // Start periodic callback to keep the connection alive, by sending
  // "0" to server
  keep_alive_handler_ = scheduler_->Schedule(
      new callback::CallbackValue1<ConnectionRef>(
          safe_this_,
          [](ConnectionRef conn_ref) {
            ConnectionRefLock lock(&conn_ref);
            auto connection = lock.GetReference();
            if (connection != nullptr && connection->client_ &&
                connection->state_ == kStateReady) {
              connection->client_->Send("0");
            }
          }),
      kKeepAliveTimeoutMs, kKeepAliveTimeoutMs);
}

void Connection::OnMessage(const char* msg) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ConnectionRefLock, lock, safe_this_);

  logger_->LogDebug("%s websocket message received", log_id_.c_str());

  HandleIncomingFrame(msg);
}

void Connection::OnClose() {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ConnectionRefLock, lock, safe_this_);

  logger_->LogDebug("%s websocket closed", log_id_.c_str());

  if (state_ != kStateDisconnected) {
    // No need to do anything if Close() has been called already.
    // Otherwise, the cause could be either connection failure or
    // connection lost, depending on whether the web socket has already
    // been connected or not.
    DisconnectReason reason = ws_connected_ ? kDisconnectReasonConnectionLost
                                            : kDisconnectReasonConnectionFailed;
    Close(reason);
  }
}

void Connection::OnError(const WebSocketClientErrorData& error_data) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ConnectionRefLock, lock, safe_this_);

  logger_->LogDebug("%s websocket error occurred.  Uri: %s", log_id_.c_str(),
                    error_data.GetUri().c_str());

  scheduler_->Schedule(new callback::CallbackValue1<ConnectionRef>(
      safe_this_, [](ConnectionRef conn_ref) {
        ConnectionRefLock lock(&conn_ref);
        auto connection = lock.GetReference();
        if (connection != nullptr) {
          // If error occurs before the connection is opened, it is due to
          // connection failed (ex. incorrect url).  Otherwise, it can be
          // any lower-level error during connection.
          DisconnectReason reason = connection->ws_connected_
                                        ? kDisconnectReasonWebsocketError
                                        : kDisconnectReasonConnectionFailed;
          connection->Close(reason);
        }
      }));
}

void Connection::HandleIncomingFrame(const char* msg) {
  if (state_ == kStateDisconnected) {
    return;
  }

  // Firebase server splits large message into multiple frames, the same way
  // how client split large message into frames before sending.  If the received
  // message is a number, this indicate how many frames to be expected in the
  // future.
  if (expected_incoming_frames_ > 0) {
    // Add msg to buffer
    incoming_buffer_ << msg;
    --expected_incoming_frames_;

    logger_->LogDebug("%s Received a frame (length: %d), %d more to come",
                      log_id_.c_str(), strlen(msg), expected_incoming_frames_);

    // If buffer is complete, process it
    if (expected_incoming_frames_ == 0) {
      ProcessMessage(incoming_buffer_.str().c_str());
    }
  } else {
    uint32_t num_of_frame = 0;
    // The server is only supposed to send up to 9999 frames (i.e. length
    // <= 4), but that isn't being enforced currently.  So allowing larger frame
    // counts (length <= 6).
    if (std::strlen(msg) <= 6) {
      int32_t parse_value = strtol(msg, nullptr, 10);  // NOLINT
      if (parse_value > 0) {
        num_of_frame = parse_value;
      }
    }

    if (num_of_frame > 0) {
      logger_->LogDebug("%s Received a frame count. Expecting %d frames later",
                        log_id_.c_str(), num_of_frame);

      // Start the buffer
      expected_incoming_frames_ = num_of_frame;
      incoming_buffer_.str("");
    } else {
      // Process it
      ProcessMessage(msg);
    }
  }
}

void Connection::ProcessMessage(const char* message) {
  Variant message_data = util::JsonToVariant(message);
  logger_->LogDebug("%s ProcessMessage (length: %d)", log_id_.c_str(),
                    strlen(message));

  FIREBASE_DEV_ASSERT(!message_data.is_null());

  const auto& messageMap = message_data.map();
  auto itType = messageMap.find(kServerEnvelopeType);

  if (itType != messageMap.end()) {
    if (itType->second.is_string()) {
      std::string type = itType->second.string_value();
      if (type == kServerDataMessage) {
        auto itData = messageMap.find(kServerEnvelopeData);
        if (itData != messageMap.end()) {
          OnDataMessage(itData->second);
        }
      } else if (type == kServerControlMessage) {
        auto itData = messageMap.find(kServerEnvelopeData);
        if (itData != messageMap.end()) {
          OnControlMessage(itData->second);
        }
      } else {
        logger_->LogDebug("%s Ignore unknown server message type: %s",
                          log_id_.c_str(), type.c_str());
      }
    } else {
      logger_->LogDebug("%s Fail to parse server message: %s", log_id_.c_str(),
                        util::VariantToJson(message_data).c_str());
      Close(kDisconnectReasonProtocolError);
    }
  } else {
    logger_->LogDebug(
        "%s Failed to parse server message: missing message type: %s",
        log_id_.c_str(), util::VariantToJson(message_data).c_str());
    Close(kDisconnectReasonProtocolError);
  }
}

void Connection::OnDataMessage(const Variant& data) {
  logger_->LogDebug("%s received data message", log_id_.c_str());

  // Do not decode data message in this level
  event_handler_->OnDataMessage(data);
}

void Connection::OnControlMessage(const Variant& data) {
  logger_->LogDebug("%s received control message: %s", log_id_.c_str(),
                    util::VariantToJson(data).c_str());

  FIREBASE_DEV_ASSERT(!data.is_null());

  const auto& data_map = data.map();
  auto itType = data_map.find(kServerControlMessageType);

  if (itType != data_map.end()) {
    if (itType->second.is_string()) {
      std::string messageType = itType->second.string_value();
      if (messageType == kServerControlMessageShutdown) {
        auto itReason = data_map.find(kServerControlMessageData);
        if (itReason != data_map.end() && itReason->second.is_string()) {
          OnConnectionShutdown(itReason->second.string_value());
        } else {
          logger_->LogDebug("%s Shut down connection for unknown reasons",
                            log_id_.c_str());
          OnConnectionShutdown("unknown");
        }
      } else if (messageType == kServerControlMessageReset) {
        auto itHost = data_map.find(kServerControlMessageData);
        if (itHost != data_map.end() && itHost->second.is_string()) {
          OnReset(itHost->second.string_value());
        } else {
          logger_->LogDebug("%s Reset connection with unknown host: %s",
                            log_id_.c_str(), util::VariantToJson(data).c_str());
          OnReset("");
        }
      } else if (messageType == kServerControlMessageHello) {
        auto itHandshake = data_map.find(kServerControlMessageData);
        if (itHandshake != data_map.end()) {
          OnHandshake(itHandshake->second);
        } else {
          logger_->LogDebug("%s Handshake received with no data: %s",
                            log_id_.c_str(), util::VariantToJson(data).c_str());
          OnHandshake(Variant());
        }
      } else if (messageType == kServerControlMessageError) {
        auto itError = data_map.find(kServerControlMessageData);
        if (itError != data_map.end() && itError->second.is_string()) {
          logger_->LogError("%s Error control message: %s", log_id_.c_str(),
                            itError->second.string_value());
        } else {
          logger_->LogError("%s Error control message with no data",
                            log_id_.c_str());
        }
      } else {
        logger_->LogDebug("%s Ignore unknown control message type: %s",
                          log_id_.c_str(), messageType.c_str());
      }
    } else {
      logger_->LogDebug("%s Fail to parse control message: %s", log_id_.c_str(),
                        util::VariantToJson(data).c_str());
      Close(kDisconnectReasonProtocolError);
    }
  } else {
    logger_->LogDebug("%s Got invalid control message: %s", log_id_.c_str(),
                      util::VariantToJson(data).c_str());
    Close(kDisconnectReasonProtocolError);
  }
}

void Connection::OnConnectionShutdown(const std::string& reason) {
  logger_->LogDebug("%s Connection shutdown command received. Shutting down...",
                    log_id_.c_str());

  event_handler_->OnKill(reason);

  Close(kDisconnectReasonShutdownMessage);
}

void Connection::OnHandshake(const Variant& handshake) {
  const auto& data_map = handshake.map();

  int64_t timestamp = 0;
  auto itTimestamp = data_map.find(kServerHelloTimestamp);
  if (itTimestamp != data_map.end()) {
    timestamp = itTimestamp->second.int64_value();
  } else {
    logger_->LogDebug("%s No timestamp from handshake message",
                      log_id_.c_str());
  }

  std::string host;
  auto itHost = data_map.find(kServerHelloHost);
  if (itHost != data_map.end()) {
    host = itHost->second.string_value();
  } else {
    logger_->LogDebug("%s No host uri from handshake message", log_id_.c_str());
  }

  event_handler_->OnCacheHost(host);

  std::string sessionId;
  auto itSession = data_map.find(kServerHelloSessionId);
  if (itSession != data_map.end()) {
    sessionId = itSession->second.string_value();
  } else {
    logger_->LogDebug("%s No session id from handshake message",
                      log_id_.c_str());
  }

  if (state_ == kStateConnecting) {
    OnConnectionReady(timestamp, sessionId);
  }
}

void Connection::OnConnectionReady(int64_t timestamp,
                                   const std::string& sessionId) {
  logger_->LogDebug("%s Connection established", log_id_.c_str());

  state_ = kStateReady;

  event_handler_->OnReady(timestamp, sessionId);
}

void Connection::OnReset(const std::string& host) {
  logger_->LogDebug(
      "%s Got a reset; killing connection to %s; Updateing internalHost to %s",
      log_id_.c_str(), host_info_.GetHost().c_str(), host.c_str());

  event_handler_->OnCacheHost(host);

  Close(kDisconnectReasonServerReset);
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
