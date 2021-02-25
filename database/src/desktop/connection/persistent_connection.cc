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

#include "database/src/desktop/connection/persistent_connection.h"

#include <algorithm>
#include <sstream>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/function_registry.h"
#include "app/src/log.h"
#include "app/src/path.h"
#include "app/src/time.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/core/constants.h"
#include "database/src/desktop/core/tag.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/common.h"

namespace firebase {
namespace database {

// This is not part of the regular Error enum because this error is not
// developer facing, and because it would be an API change to add it. We
// can always decide to add it to the public API later if we want.
Error kErrorDataStale = static_cast<Error>(-1);

namespace internal {
namespace connection {

const char* PersistentConnection::kRequestError = "error";
const char* PersistentConnection::kRequestQueries = "q";
const char* PersistentConnection::kRequestTag = "t";
const char* PersistentConnection::kRequestStatus = "s";
const char* PersistentConnection::kRequestStatusOk = "ok";
const char* PersistentConnection::kRequestPath = "p";
const char* PersistentConnection::kRequestNumber = "r";
const char* PersistentConnection::kRequestPayload = "b";
const char* PersistentConnection::kRequestCounters = "c";
const char* PersistentConnection::kRequestDataPayload = "d";
const char* PersistentConnection::kRequestDataHash = "h";
const char* PersistentConnection::kRequestCompoundHash = "ch";
const char* PersistentConnection::kRequestCompoundHashPaths = "ps";
const char* PersistentConnection::kRequestCompoundHashHashes = "hs";
const char* PersistentConnection::kRequestCredential = "cred";
const char* PersistentConnection::kRequestAuthVar = "authvar";
const char* PersistentConnection::kRequestAction = "a";
const char* PersistentConnection::kRequestActionStats = "s";
const char* PersistentConnection::kRequestActionQuery = "q";
const char* PersistentConnection::kRequestActionPut = "p";
const char* PersistentConnection::kRequestActionMerge = "m";
const char* PersistentConnection::kRequestActionQueryUnlisten = "n";
const char* PersistentConnection::kRequestActionOnDisconnectPut = "o";
const char* PersistentConnection::kRequestActionOnDisconnectMerge = "om";
const char* PersistentConnection::kRequestActionOnDisconnectCancel = "oc";
const char* PersistentConnection::kRequestActionAuth = "auth";
const char* PersistentConnection::kRequestActionGauth = "gauth";
const char* PersistentConnection::kRequestActionUnauth = "unauth";
const char* PersistentConnection::kRequestNoAuth = "noauth";
const char* PersistentConnection::kResponseForRequest = "b";
const char* PersistentConnection::kServerAsyncAction = "a";
const char* PersistentConnection::kServerAsyncPayload = "b";
const char* PersistentConnection::kServerAsyncDataUpdate = "d";
const char* PersistentConnection::kServerAsyncDataMerge = "m";
const char* PersistentConnection::kServerAsyncDataRangeMerge = "rm";
const char* PersistentConnection::kServerAsyncAuthRevoked = "ac";
const char* PersistentConnection::kServerAsyncListenCancelled = "c";
const char* PersistentConnection::kServerAsyncSecurityDebug = "sd";
const char* PersistentConnection::kServerDataUpdatePath = "p";
const char* PersistentConnection::kServerDataUpdateBody = "d";
const char* PersistentConnection::kServerDataStartPath = "s";
const char* PersistentConnection::kServerDataEndPath = "e";
const char* PersistentConnection::kServerDataRangeMerge = "m";
const char* PersistentConnection::kServerDataTag = "t";
const char* PersistentConnection::kServerDataWarnings = "w";
const char* PersistentConnection::kServerResponseData = "d";

int PersistentConnection::kInvalidAuthTokenThreshold = 3;

compat::Atomic<uint32_t> PersistentConnection::next_log_id_(0);

// Util function to print QuerySpec in debug logs.
std::string GetDebugQuerySpecString(const QuerySpec& query_spec) {
  std::stringstream ss;
  ss << WireProtocolPathToString(query_spec.path) << " params: "
     << util::VariantToJson(GetWireProtocolParams(query_spec.params)) << ")";
  return ss.str();
}

PersistentConnection::PersistentConnection(
    App* app, const HostInfo& info,
    PersistentConnectionEventHandler* event_handler,
    scheduler::Scheduler* scheduler, Logger* logger)
    : app_(app),
      safe_this_(this),
      scheduler_(scheduler),
      host_info_(info),
      event_handler_(event_handler),
      realtime_(nullptr),
      connection_state_(kDisconnected),
      is_first_connection_(true),
      invalid_auth_token_count(0),
      next_request_id_(0),
      force_auth_refresh_(false),
      next_listen_id_(0),
      next_write_id_(0),
      logger_(logger) {
  FIREBASE_DEV_ASSERT(app);
  FIREBASE_DEV_ASSERT(scheduler);
  FIREBASE_DEV_ASSERT(event_handler_);

  // Create log id like "[pc_0]" for debugging
  std::stringstream log_id_stream;
  log_id_stream << "[pc_" << next_log_id_.fetch_add(1) << "]";
  log_id_ = log_id_stream.str();
}

PersistentConnection::~PersistentConnection() {
  // Clear safe reference immediately so that scheduled callback can skip
  // executing code which requires reference to this.
  safe_this_.ClearReference();

  // Clear OnCompletion function for pending token future
  {
    MutexLock future_lock(pending_token_future_mutex_);
    if (pending_token_future_.status() != kFutureStatusInvalid) {
      pending_token_future_.OnCompletion(nullptr, nullptr);
      pending_token_future_ = Future<std::string>();
    }
  }

  // Destroy the client so that no more event will be triggered from this point.
  realtime_.reset(nullptr);
}

void PersistentConnection::OnCacheHost(const std::string& host) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);

  // TODO(chkuang): Ignore cache host for now.
}

std::string GetStringValue(const Variant& data, const char* key,
                           bool force = false) {
  if (!data.is_map()) {
    return "";
  }

  auto itValue = data.map().find(key);
  if (itValue == data.map().end()) {
    return "";
  }

  if (itValue->second.is_string()) {
    return itValue->second.string_value();
  } else if (force) {
    return util::VariantToJson(itValue->second);
  }
  return "";
}

bool HasKey(const Variant& data, const char* key) {
  if (!data.is_map()) {
    return false;
  }

  return data.map().find(key) != data.map().end();
}

void PersistentConnection::OnReady(int64_t timestamp,
                                   const std::string& session_id) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);

  logger_->LogDebug("%s OnReady", log_id_.c_str());

  // Trigger OnServerInfoUpdate based on timestamp delta
  logger_->LogDebug("%s Handle timestamp: %lld in ms", log_id_.c_str(),
                    timestamp);
  int64_t time_delta = timestamp - ::firebase::internal::GetTimestampEpoch();
  std::map<Variant, Variant> updates{
      std::make_pair(kDotInfoServerTimeOffset, time_delta),
  };
  event_handler_->OnServerInfoUpdate(updates);

  // Send client SDK status
  if (is_first_connection_) {
    Variant stats = Variant::EmptyMap();
    stats.map()[host_info_.web_socket_user_agent()] = 1;
    logger_->LogDebug("%s Sending first connection stats", log_id_.c_str());
    Variant request = Variant::EmptyMap();
    request.map()[kRequestCounters] = stats;
    SendSensitive(kRequestActionStats, false, request, ResponsePtr(),
                  &PersistentConnection::HandleConnectStatsResponse, 0);
  }
  is_first_connection_ = false;

  // Restore Auth
  logger_->LogDebug("%s calling restore state", log_id_.c_str());
  FIREBASE_DEV_ASSERT(connection_state_ == kConnecting);

  // Try to retrieve auth token synchronously when connection is ready.
  GetAuthToken(&auth_token_);

  if (auth_token_.empty()) {
    logger_->LogDebug("%s Not restoring auth because token is null.",
                      log_id_.c_str());
    connection_state_ = kConnected;
    RestoreOutstandingRequests();
  } else {
    logger_->LogDebug("%s Restoring auth", log_id_.c_str());
    connection_state_ = kAuthenticating;
    // Only need to restore outstanding if it is sent from OnReady() since
    // all the request are deferred for auth message
    SendAuthToken(auth_token_, true);
  }

  last_session_id_ = session_id;

  // Trigger OnConnect() event
  event_handler_->OnConnect();
}

void PersistentConnection::HandleConnectStatsResponse(
    const Variant& message, const ResponsePtr& response,
    uint64_t outstanding_id) {
  auto status = GetStringValue(message, kRequestStatus);
  if (status != kRequestStatusOk) {
    auto error = GetStringValue(message, kServerDataUpdateBody, true);

    if (GetLogLevel() > kLogLevelInfo) {
      logger_->LogDebug("%s Failed to send stats: %s  (message: %s)",
                        log_id_.c_str(), status.c_str(), error.c_str());
    }
  }
}

void PersistentConnection::OnDataMessage(const Variant& message) {
  FIREBASE_DEV_ASSERT(message.is_map());

  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);

  if (HasKey(message, kRequestNumber)) {
    auto it_request_number = message.map().find(kRequestNumber);
    FIREBASE_DEV_ASSERT(it_request_number->second.is_numeric());
    uint64_t rn = it_request_number->second.int64_value();

    RequestDataPtr request_ptr;
    auto it_request = request_map_.find(rn);
    FIREBASE_DEV_ASSERT(it_request != request_map_.end());
    if (it_request != request_map_.end()) {
      request_ptr = Move(it_request->second);
      request_map_.erase(it_request);
    }
    FIREBASE_DEV_ASSERT(request_ptr);
    if (request_ptr) {
      auto it_response_message = message.map().find(kResponseForRequest);
      FIREBASE_DEV_ASSERT(it_response_message != message.map().end());
      if (it_response_message != message.map().end()) {
        logger_->LogDebug("%s Trigger handler for request %llu",
                          log_id_.c_str(), rn);
        if (request_ptr->callback) {
          (*this.*request_ptr->callback)(it_response_message->second,
                                         request_ptr->response,
                                         request_ptr->outstanding_id);
        }
      }
    }
  } else if (HasKey(message, kRequestError)) {
    logger_->LogError("%s Received Error Data Message: %s", log_id_.c_str(),
                      GetStringValue(message, kRequestError, true).c_str());
  } else if (HasKey(message, kServerAsyncAction)) {
    auto* action = GetInternalVariant(&message, kServerAsyncAction);
    if (!action || !action->is_string())
      logger_->LogError("Received Server Async Action is not a string.");

    auto* body = GetInternalVariant(&message, kServerAsyncPayload);
    if (action && body) {
      OnDataPush(action->string_value(), *body);
    }
  } else {
    logger_->LogDebug("%s Ignoring unknown message: %s", log_id_.c_str(),
                      util::VariantToJson(message).c_str());
  }
}

void PersistentConnection::OnDisconnect(Connection::DisconnectReason reason) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);

  logger_->LogDebug("%s Got on disconnect due to %d", log_id_.c_str(),
                    static_cast<int>(reason));

  connection_state_ = kDisconnected;
  realtime_.reset(nullptr);

  request_map_.clear();

  // TODO(chkuang): Implement Idle Check
  // this.hasOnDisconnects = false;
  //  if (inactivityTimer != null) {
  //    logger_->LogDebug("%s cancelling idle time checker", log_id_.c_str());
  //    inactivityTimer.Cancel();
  //  }

  CancelSentTransactions();

  if (ShouldReconnect()) {
    TryScheduleReconnect();
  }

  // Trigger OnDisconnect event
  event_handler_->OnDisconnect();
}

void PersistentConnection::OnKill(const std::string& reason) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);

  logger_->LogDebug(
      "%s Firebase Database connection was forcefully killed by the server. "
      "Will not attempt reconnect. Reason: %s",
      log_id_.c_str(), reason.c_str());
  InterruptInternal(kInterruptServerKill);
}

void PersistentConnection::ScheduleInitialize() {
  scheduler_->Schedule(
      new callback::CallbackValue1<ThisRef>(safe_this_, [](ThisRef ref) {
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->TryScheduleReconnect();
        }
      }));
}

void PersistentConnection::ScheduleShutdown() {
  scheduler_->Schedule(
      new callback::CallbackValue1<ThisRef>(safe_this_, [](ThisRef ref) {
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->InterruptInternal(kInterruptShutdown);
        }
      }));
}

void PersistentConnection::Listen(const QuerySpec& query_spec, const Tag& tag,
                                  ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  logger_->LogDebug("%s Listening on %s", log_id_.c_str(),
                    GetDebugQuerySpecString(query_spec).c_str());

  FIREBASE_DEV_ASSERT_MESSAGE(listens_.find(query_spec) == listens_.end(),
                              "Listen() called twice for same QuerySpec. %s",
                              GetDebugQuerySpecString(query_spec).c_str());

  // listen_id is used to search for QuerySpec later when the response message
  // is received.
  uint64_t listen_id = next_listen_id_++;
  auto it = listens_.insert(Move(std::pair<QuerySpec, OutstandingListenPtr>(
      query_spec, Move(MakeUnique<OutstandingListen>(query_spec, tag, response,
                                                     listen_id)))));
  listen_id_to_query_[listen_id] = query_spec;

  // If the connection is established, send the request immediately.  Otherwise,
  // wait for RestoreOutstandingRequests() being called.
  if (IsConnected()) {
    SendListen(*it.first->second);
  }
}

void PersistentConnection::Unlisten(const QuerySpec& query_spec) {
  CheckAuthTokenAndSendOnChange();
  logger_->LogDebug("%s Unlisten on %s", log_id_.c_str(),
                    GetDebugQuerySpecString(query_spec).c_str());

  OutstandingListenPtr listen = Move(RemoveListen(query_spec));

  // If the connection is established, send the request immediately.  Otherwise,
  // do nothing because all listen request is cancelled when disconnected.
  if (listen && IsConnected()) {
    SendUnlisten(*listen);
  }
}

void PersistentConnection::Put(const Path& path, const Variant& data,
                               ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  PutInternal(kRequestActionPut, path, data, /*hash=*/nullptr, Move(response));
}

void PersistentConnection::CompareAndPut(const Path& path, const Variant& data,
                                         const std::string& hash,
                                         ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  PutInternal(kRequestActionPut, path, data, hash.c_str(), Move(response));
}

void PersistentConnection::Merge(const Path& path, const Variant& data,
                                 ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  PutInternal(kRequestActionMerge, path, data, nullptr, Move(response));
}

void PersistentConnection::PurgeOutstandingWrites() {
  // Purge outstanding put requests
  for (auto& put : outstanding_puts_) {
    TriggerResponse(put.second->response, kErrorWriteCanceled,
                    GetErrorMessage(kErrorWriteCanceled));
  }
  outstanding_puts_.clear();

  // Purge outstanding OnDisconnect requests
  while (!outstanding_ondisconnects_.empty()) {
    OutstandingOnDisconnectPtr ondisconnect =
        Move(outstanding_ondisconnects_.front());
    outstanding_ondisconnects_.pop();

    TriggerResponse(ondisconnect->response, kErrorWriteCanceled,
                    GetErrorMessage(kErrorWriteCanceled));
  }
}

void PersistentConnection::OnDisconnectPut(const Path& path,
                                           const Variant& data,
                                           ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  if (CanSendWrites()) {
    SendOnDisconnect(kRequestActionOnDisconnectPut, path, data, Move(response));
  } else {
    outstanding_ondisconnects_.push(MakeUnique<OutstandingOnDisconnect>(
        kRequestActionOnDisconnectPut, path, data, Move(response)));
  }
}

void PersistentConnection::OnDisconnectMerge(const Path& path,
                                             const Variant& updates,
                                             ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  if (CanSendWrites()) {
    SendOnDisconnect(kRequestActionOnDisconnectMerge, path, updates,
                     Move(response));
  } else {
    outstanding_ondisconnects_.push(MakeUnique<OutstandingOnDisconnect>(
        kRequestActionOnDisconnectMerge, path, updates, Move(response)));
  }
}

void PersistentConnection::OnDisconnectCancel(const Path& path,
                                              ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  if (CanSendWrites()) {
    SendOnDisconnect(kRequestActionOnDisconnectCancel, path, Variant::Null(),
                     Move(response));
  } else {
    outstanding_ondisconnects_.push(MakeUnique<OutstandingOnDisconnect>(
        kRequestActionOnDisconnectCancel, path, Variant::Null(),
        Move(response)));
  }
}

void PersistentConnection::Interrupt() { InterruptInternal(kInterruptManual); }

void PersistentConnection::Resume() { ResumeInternal(kInterruptManual); }

bool PersistentConnection::IsInterrupted() {
  return IsInterruptedInternal(kInterruptManual);
}

void PersistentConnection::InterruptInternal(InterruptReason reason) {
  logger_->LogDebug("%s Connection interrupted for: %d", log_id_.c_str(),
                    static_cast<int>(reason));

  interrupt_reasons_.insert(reason);

  if (realtime_) {
    realtime_->Close();
    realtime_.reset(nullptr);
  } else {
    // TODO(chkuang): Implement Retry
    // retryHelper.cancel();
    connection_state_ = kDisconnected;
  }
  // TODO(chkuang): Implement Retry
  // retryHelper.signalSuccess();
}

void PersistentConnection::ResumeInternal(InterruptReason reason) {
  logger_->LogDebug("%s Connection no longer interrupted for: %d",
                    log_id_.c_str(), static_cast<int>(reason));

  interrupt_reasons_.erase(reason);

  if (ShouldReconnect() && connection_state_ == kDisconnected) {
    TryScheduleReconnect();
  }
}

void PersistentConnection::TryScheduleReconnect() {
  if (!ShouldReconnect()) {
    return;
  }

  FIREBASE_DEV_ASSERT(connection_state_ == kDisconnected);
  bool force_refresh = force_auth_refresh_;
  force_auth_refresh_ = false;
  logger_->LogDebug("%s Scheduling connection attempt", log_id_.c_str());

  scheduler_->Schedule(new callback::CallbackValue2<ThisRef, bool>(
      safe_this_, force_refresh, [](ThisRef ref, bool force_refresh) {
        ThisRefLock lock(&ref);
        auto* connection = lock.GetReference();
        if (!connection) return;
        // TODO(chkuang): Implement Exponential Backoff Retry
        connection->connection_state_ = kGettingToken;
        connection->logger_->LogDebug("%s Trying to fetch auth token",
                                      connection->log_id_.c_str());

        // Get Token Asynchronously to make sure the token is not expired.
        Future<std::string> future;
        bool succeeded = connection->app_->function_registry()->CallFunction(
            ::firebase::internal::FnAuthGetTokenAsync, connection->app_,
            &force_refresh, &future);
        if (succeeded && future.status() != kFutureStatusInvalid) {
          // Set pending future
          MutexLock future_lock(connection->pending_token_future_mutex_);
          connection->pending_token_future_ = future;
          future.OnCompletion(OnTokenFutureComplete, connection);
        } else {
          // Auth is not available now.  Start the connection anyway.
          connection->OpenNetworkConnection();
        }
      }));
}

void PersistentConnection::OnTokenFutureComplete(
    const Future<std::string>& result_data, void* user_data) {
  FIREBASE_DEV_ASSERT(user_data);
  PersistentConnection* connection =
      static_cast<PersistentConnection*>(user_data);
  ThisRefLock lock(&connection->safe_this_);
  // If the connection is destroyed or being destroyed, do nothing.
  if (!lock.GetReference()) return;

  {
    // Clear pending future
    MutexLock future_lock(connection->pending_token_future_mutex_);
    connection->pending_token_future_ = Future<std::string>();
  }

  connection->scheduler_->Schedule(
      new callback::CallbackValue2<ThisRef, Future<std::string>>(
          connection->safe_this_, result_data,
          [](ThisRef ref, Future<std::string> future) {
            ThisRefLock lock(&ref);
            if (lock.GetReference()) {
              lock.GetReference()->HandleTokenFuture(future);
            }
          }));
}

void PersistentConnection::HandleTokenFuture(Future<std::string> future) {
  if (future.error() == 0) {
    if (connection_state_ == kGettingToken) {
      logger_->LogDebug("%s Successfully fetched token, opening connection",
                        log_id_.c_str());
      auth_token_ = *future.result();
      OpenNetworkConnection();
    } else {
      FIREBASE_DEV_ASSERT(connection_state_ == kDisconnected);
      logger_->LogDebug(
          "%s Not opening connection after token refresh, because "
          "connection was set to disconnected",
          log_id_.c_str());
    }
  } else {
    connection_state_ = kDisconnected;
    logger_->LogDebug("%s Error fetching token: %s", log_id_.c_str(),
                      future.error_message());
    TryScheduleReconnect();
  }
}

void PersistentConnection::OpenNetworkConnection() {
  FIREBASE_DEV_ASSERT(connection_state_ == kGettingToken);

  // User might have logged out. Positive auth status is handled after
  // authenticating with the server
  if (auth_token_.empty()) {
    // Trigger OnAuthStatus(false) event
    event_handler_->OnAuthStatus(false);
  }

  connection_state_ = kConnecting;

  realtime_ = MakeUnique<Connection>(
      scheduler_, host_info_,
      last_session_id_.empty() ? nullptr : last_session_id_.c_str(), this,
      logger_);
  realtime_->Open();
}

void PersistentConnection::SendListen(const OutstandingListen& listen) {
  Variant request = Variant::EmptyMap();
  auto& map = request.map();
  map[kRequestPath] = WireProtocolPathToString(listen.query_spec.path);

  if (listen.tag.has_value()) {
    Variant params = GetWireProtocolParams(listen.query_spec.params);
    map[kRequestQueries] = params;
    map[kRequestTag] = listen.tag.value();
  }

  // TODO(chkuang): Support Hash and Compound Hash.
  //                The server always send one DataUpdate message to client now.
  // map[kRequestDataHash] = "";

  SendSensitive(kRequestActionQuery, false, request, listen.response,
                &PersistentConnection::HandleListenResponse,
                listen.outstanding_id);
}

void PersistentConnection::SendUnlisten(const OutstandingListen& listen) {
  Variant request = Variant::EmptyMap();
  auto& map = request.map();
  map[kRequestPath] = WireProtocolPathToString(listen.query_spec.path);

  if (listen.tag.has_value()) {
    Variant params = GetWireProtocolParams(listen.query_spec.params);
    map[kRequestQueries] = params;
    map[kRequestTag] = listen.tag.value();
  }

  SendSensitive(kRequestActionQueryUnlisten, false, request, ResponsePtr(),
                nullptr, 0);
}

void PersistentConnection::HandleListenResponse(const Variant& message,
                                                const ResponsePtr& response,
                                                uint64_t listen_id) {
  auto it_spec = listen_id_to_query_.find(listen_id);
  if (it_spec == listen_id_to_query_.end()) {
    logger_->LogDebug(
        "%s Listen Id has been removed.  Do nothing. response: %s",
        log_id_.c_str(), util::VariantToJson(message).c_str());
    return;
  }

  auto it_listen = listens_.find(it_spec->second);
  if (it_listen == listens_.end()) {
    logger_->LogDebug(
        "%s Listen Request for %s has been removed.  Do nothing. response: %s",
        log_id_.c_str(), GetDebugQuerySpecString(it_spec->second).c_str(),
        util::VariantToJson(message).c_str());
    return;
  }

  logger_->LogDebug("%s Listen response: %s", log_id_.c_str(),
                    util::VariantToJson(message).c_str());

  std::string status_string = GetStringValue(message, kRequestStatus);
  Error error_code = StatusStringToErrorCode(status_string);
  bool is_ok = error_code == kErrorNone;

  // Warn if the developer listen on a unspecified index.
  if (is_ok) {
    const Variant* server_body =
        GetInternalVariant(&message, Variant(kServerDataUpdateBody));
    const Variant* server_warning =
        server_body
            ? GetInternalVariant(server_body, Variant(kServerDataWarnings))
            : nullptr;
    if (server_warning != nullptr) {
      WarnOnListenerWarnings(*server_warning, it_spec->second);
    }
  } else {
    RemoveListen(it_spec->second);
  }

  TriggerResponse(
      response, error_code,
      is_ok ? "" : GetStringValue(message, kServerDataUpdateBody, true));
}

void PersistentConnection::WarnOnListenerWarnings(const Variant& warnings,
                                                  const QuerySpec& query_spec) {
  if (warnings.is_vector()) {
    auto it_no_index_warning =
        std::find(warnings.vector().begin(), warnings.vector().end(),
                  Variant("no_index"));
    if (it_no_index_warning != warnings.vector().end()) {
      Variant wire_protocol = GetWireProtocolParams(query_spec.params);
      const Variant* index_on =
          GetInternalVariant(&wire_protocol, Variant("i"));
      logger_->LogWarning(
          "%s Using an unspecified index. Consider adding '\".indexOn\": "
          "\"%s\"' at %s to your security and Firebase Database rules for "
          "better performance",
          log_id_.c_str(),
          index_on != nullptr && index_on->is_string()
              ? index_on->string_value()
              : "NULL",
          WireProtocolPathToString(query_spec.path).c_str());
    }
  }
}

PersistentConnection::OutstandingListenPtr PersistentConnection::RemoveListen(
    const QuerySpec& query_spec) {
  logger_->LogDebug("%s Removing query %s", log_id_.c_str(),
                    GetDebugQuerySpecString(query_spec).c_str());

  auto it_listen = listens_.find(query_spec);
  if (it_listen == listens_.end()) {
    logger_->LogDebug(
        "%s Trying to remove listener for QuerySpec %s but no listener exists.",
        log_id_.c_str(), GetDebugQuerySpecString(query_spec).c_str());
    return OutstandingListenPtr();
  } else {
    OutstandingListenPtr listen_ptr = Move(it_listen->second);
    listens_.erase(it_listen);
    listen_id_to_query_.erase(listen_ptr->outstanding_id);
    return Move(listen_ptr);
  }
}

void PersistentConnection::OnDataPush(const std::string& action,
                                      const Variant& body) {
  logger_->LogDebug("%s handleServerMessage %s %s", log_id_.c_str(),
                    action.c_str(), util::VariantToJson(body).c_str());

  if (action == kServerAsyncDataUpdate || action == kServerAsyncDataMerge) {
    bool is_merge = action.compare(kServerAsyncDataMerge) == 0;
    auto* path_variant = GetInternalVariant(&body, kServerDataUpdatePath);
    if (!path_variant)
      logger_->LogError("Received path from Server Async Action is missing.");
    auto* payload_data = GetInternalVariant(&body, kServerDataUpdateBody);
    if (!payload_data)
      logger_->LogError(
          "Received payload data from Server Async Action is missing.");
    auto* tag_variant = GetInternalVariant(&body, kServerDataTag);

    // Ignore empty merges
    if (is_merge && payload_data != nullptr && payload_data->is_map() &&
        payload_data->map().empty()) {
      logger_->LogDebug("%s ignoring empty merge for path %s", log_id_.c_str(),
                        path_variant->AsString().string_value());
    } else {
      Path path(path_variant->AsString().string_value());
      event_handler_->OnDataUpdate(
          path, *payload_data, is_merge,
          tag_variant ? Tag(tag_variant->AsInt64().int64_value()) : Tag());
    }
  } else if (action.compare(kServerAsyncDataRangeMerge) == 0) {
    // TODO(chkuang): Support Compound Hash
  } else if (action.compare(kServerAsyncListenCancelled) == 0) {
    auto* path = GetInternalVariant(&body, kServerDataUpdatePath);
    if (path) {
      OnListenRevoked(Path(path->AsString().string_value()));
    }
  } else if (action.compare(kServerAsyncAuthRevoked) == 0) {
    auto* status = GetInternalVariant(&body, kRequestStatus);
    auto* reason = GetInternalVariant(&body, kServerDataUpdateBody);
    Error error_code =
        status ? StatusStringToErrorCode(status->AsString().string_value())
               : kErrorUnknownError;
    OnAuthRevoked(error_code,
                  reason ? reason->AsString().string_value() : "null");

  } else if (action.compare(kServerAsyncSecurityDebug) == 0) {
    auto* msg = GetInternalVariant(&body, "msg");
    if (msg) {
      logger_->LogInfo("%s %s", log_id_.c_str(),
                       util::VariantToJson(*msg).c_str());
    }
  } else {
    logger_->LogDebug("%s Unrecognized action from server: %s", log_id_.c_str(),
                      util::VariantToJson(action).c_str());
  }
}

void PersistentConnection::OnListenRevoked(const Path& path) {
  std::vector<ResponsePtr> responses_to_trigger;

  // Remove all outstanding listens with the given path.
  auto it = listens_.begin();
  while (it != listens_.end()) {
    auto& query_spec = it->first;
    auto& outstanding_listen_ptr = it->second;
    if (query_spec.path == path) {
      responses_to_trigger.push_back(outstanding_listen_ptr->response);
      it = listens_.erase(it);
    } else {
      ++it;
    }
  }

  // Trigger responses with permission_denied error code.
  for (auto& response : responses_to_trigger) {
    TriggerResponse(response, kErrorPermissionDenied,
                    GetErrorMessage(kErrorPermissionDenied));
  }
}

void PersistentConnection::PutInternal(const char* action, const Path& path,
                                       const Variant& data, const char* hash,
                                       ResponsePtr response) {
  Variant request = Variant::EmptyMap();
  request.map()[kRequestPath] = path.str();
  request.map()[kRequestDataPayload] = data;
  if (hash != nullptr) {
    request.map()[kRequestDataHash] = std::string(hash);
  }

  uint64_t write_id = next_write_id_++;
  outstanding_puts_[write_id] =
      MakeUnique<OutstandingPut>(action, request, response);

  if (CanSendWrites()) {
    SendPut(write_id);
  }
}

void PersistentConnection::SendPut(uint64_t write_id) {
  FIREBASE_DEV_ASSERT(CanSendWrites());

  auto it_put = outstanding_puts_.find(write_id);
  FIREBASE_DEV_ASSERT(it_put != outstanding_puts_.end());

  it_put->second->MarkSent();
  SendSensitive(it_put->second->action.c_str(), false, it_put->second->data,
                it_put->second->response,
                &PersistentConnection::HandlePutResponse, write_id);
}

void PersistentConnection::HandlePutResponse(const Variant& message,
                                             const ResponsePtr& response,
                                             uint64_t outstanding_id) {
  auto it_put = outstanding_puts_.find(outstanding_id);
  if (it_put != outstanding_puts_.end()) {
    auto& put_ptr = it_put->second;
    logger_->LogDebug("%s %s response: %s", log_id_.c_str(),
                      put_ptr->action.c_str(),
                      util::VariantToJson(message).c_str());
    std::string status_string = GetStringValue(message, kRequestStatus);
    Error error_code = StatusStringToErrorCode(status_string);
    bool is_ok = error_code == kErrorNone;

    TriggerResponse(
        response, error_code,
        is_ok ? "" : GetStringValue(message, kServerDataUpdateBody, true));
    outstanding_puts_.erase(it_put);
  } else {
    logger_->LogDebug(
        "%s Ignore on complete for put (%llu) because it was removed already.",
        log_id_.c_str(), outstanding_id);
  }
}

void PersistentConnection::CancelSentTransactions() {
  std::vector<OutstandingPutPtr> cancelled_transaction_writes;
  for (auto it_put = outstanding_puts_.begin();
       it_put != outstanding_puts_.end();) {
    if (it_put->second->data.map().find(kRequestDataHash) !=
            it_put->second->data.map().end() &&
        it_put->second->WasSent()) {
      cancelled_transaction_writes.push_back(Move(it_put->second));
      outstanding_puts_.erase(it_put);
    } else {
      ++it_put;
    }
  }
  for (auto& put : cancelled_transaction_writes) {
    TriggerResponse(put->response, kErrorDisconnected,
                    GetErrorMessage(kErrorDisconnected));
  }
}

void PersistentConnection::SendOnDisconnect(const char* action,
                                            const Path& path,
                                            const Variant& data,
                                            ResponsePtr response) {
  // TODO(chkuang): Validate path format

  Variant request = Variant::EmptyMap();
  request.map()[kRequestPath] = path.str();
  request.map()[kRequestDataPayload] = data;

  SendSensitive(action, false, request, Move(response),
                &PersistentConnection::HandleOnDisconnectResponse, 0);
}

void PersistentConnection::HandleOnDisconnectResponse(
    const Variant& message, const ResponsePtr& response,
    uint64_t outstanding_id) {
  std::string status_string = GetStringValue(message, kRequestStatus);
  Error error_code = StatusStringToErrorCode(status_string);
  bool is_ok = error_code == kErrorNone;

  TriggerResponse(
      response, error_code,
      is_ok ? "" : GetStringValue(message, kServerDataUpdateBody, true));
}

void PersistentConnection::SendSensitive(const char* action, bool sensitive,
                                         const Variant& message,
                                         ResponsePtr response,
                                         ConnectionResponseHandler callback,
                                         uint64_t outstanding_id) {
  FIREBASE_DEV_ASSERT(realtime_);

  // Varient only accept int64_t
  int64_t rn = ++next_request_id_;
  Variant request = Variant::EmptyMap();
  request.map()[kRequestNumber] = rn;
  request.map()[kRequestAction] = action;
  request.map()[kRequestPayload] = message;
  realtime_->Send(request, sensitive);

  // TODO(chkuang): Add timeout handle
  request_map_[rn] =
      MakeUnique<RequestData>(Move(response), callback, outstanding_id);
}

void PersistentConnection::RestoreOutstandingRequests() {
  FIREBASE_DEV_ASSERT(connection_state_ == kConnected);

  // Restore listens
  logger_->LogDebug("%s Restoring outstanding listens", log_id_.c_str());
  for (auto& it_listen : listens_) {
    logger_->LogDebug(
        "%s Restoring listen %s", log_id_.c_str(),
        GetDebugQuerySpecString(it_listen.second->query_spec).c_str());
    SendListen(*it_listen.second);
  }

  // Restore puts
  for (auto& it_put : outstanding_puts_) {
    SendPut(it_put.first);
  }

  // Restore disconnect operations
  while (!outstanding_ondisconnects_.empty()) {
    auto& front = outstanding_ondisconnects_.front();
    SendOnDisconnect(front->action.c_str(), front->path, front->data,
                     Move(front->response));
    outstanding_ondisconnects_.pop();
  }
}

void PersistentConnection::GetAuthToken(std::string* out) {
  FIREBASE_DEV_ASSERT(out);
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetCurrentToken, app_, nullptr, out);
}

void PersistentConnection::CheckAuthTokenAndSendOnChange() {
  std::string old_token = auth_token_;

  GetAuthToken(&auth_token_);

  if (auth_token_ != old_token) {
    // If connected, send the token immediately.  Otherwise, wait until
    // OnReady() is triggered.
    if (IsConnected()) {
      if (!auth_token_.empty()) {
        // No need to restore outstanding if the auth token is refreshed when
        // connected
        SendAuthToken(auth_token_, false);
      } else {
        SendUnauth();
      }
    }
  }
}

// This response class is just used to capture restore_outstanding_on_response
// flag for SendAuthToken request.
class SendAuthResponse : public Response {
 public:
  explicit SendAuthResponse(bool restore_outstanding_on_response)
      : Response(nullptr),
        restore_outstanding_on_response_(restore_outstanding_on_response) {}

  bool GetRestoreOutstandingsFlag() const {
    return restore_outstanding_on_response_;
  }

 private:
  bool restore_outstanding_on_response_;
};

void PersistentConnection::SendAuthToken(const std::string& token,
                                         bool restore_outstanding_on_response) {
  logger_->LogDebug("%s Sending auth token", log_id_.c_str());
  Variant request = Variant::EmptyMap();
  request.map()[kRequestCredential] = token;
  SendSensitive(kRequestActionAuth, true, request,
                MakeShared<SendAuthResponse>(restore_outstanding_on_response),
                &PersistentConnection::HandleAuthTokenResponse, 0);
}

void PersistentConnection::SendUnauth() {
  logger_->LogDebug("%s Sending unauth", log_id_.c_str());
  SendSensitive(kRequestActionUnauth, false, Variant::EmptyMap(), ResponsePtr(),
                nullptr, 0);
}

void PersistentConnection::HandleAuthTokenResponse(const Variant& message,
                                                   const ResponsePtr& response,
                                                   uint64_t outstanding_id) {
  FIREBASE_DEV_ASSERT(response);

  connection_state_ = kConnected;

  std::string status = GetStringValue(message, kRequestStatus);

  if (status == kRequestStatusOk) {
    invalid_auth_token_count = 0;
    event_handler_->OnAuthStatus(true);
    logger_->LogDebug("%s Authentication success", log_id_.c_str());

    SendAuthResponse* send_auth_response =
        static_cast<SendAuthResponse*>(response.get());

    if (send_auth_response->GetRestoreOutstandingsFlag()) {
      RestoreOutstandingRequests();
    }
  } else {
    auth_token_.clear();
    force_auth_refresh_ = true;
    event_handler_->OnAuthStatus(false);

    std::string reason = GetStringValue(message, kServerResponseData);
    logger_->LogDebug("%s Authentication failed: %s (%s)", log_id_.c_str(),
                      status.c_str(), reason.c_str());
    realtime_->Close();

    Error error = StatusStringToErrorCode(status);
    if (error == kErrorInvalidToken) {
      // We'll wait a couple times before logging the warning / increasing the
      // retry period since oauth tokens will report as "invalid" if they're
      // just expired. Plus there may be transient issues that resolve
      // themselves.
      ++invalid_auth_token_count;

      if (invalid_auth_token_count >= kInvalidAuthTokenThreshold) {
        // TODO(chkuang): Maximize retry interval (after retry is properly
        //                implemented).
        logger_->LogWarning(
            "Provided authentication credentials are invalid. This indicates "
            "your FirebaseApp instance was not initialized correctly. Make "
            "sure your google-services.json file has the correct firebase_url "
            "and api_key. You can re-download google-services.json from "
            "https://console.firebase.google.com/.");
      }
    }
  }
}

void PersistentConnection::OnAuthRevoked(Error error_code,
                                         const std::string& reason) {
  // This might be for an earlier token than we just recently sent. But since we
  // need to close the connection anyways, we can set it to null here and we
  // will refresh the token later on reconnect.
  logger_->LogDebug("%s Auth token revoked: %i (%s)", log_id_.c_str(),
                    error_code, reason.c_str());
  auth_token_ = "";
  force_auth_refresh_ = true;
  event_handler_->OnAuthStatus(false);
  // Close connection and reconnect
  realtime_->Close();
}

void PersistentConnection::TriggerResponse(const ResponsePtr& response_ptr,
                                           Error error_code,
                                           const std::string& error_message) {
  if (response_ptr) {
    response_ptr->error_code_ = error_code;
    response_ptr->error_message_ = error_message;
    if (response_ptr->callback_) {
      response_ptr->callback_(response_ptr);
    }
  }
}

static const struct ErrorMap {
  const char* error_string;
  Error error_code;
} g_error_codes[] = {
    {"ok", kErrorNone},
    {"datastale", kErrorDataStale},
    {"failure", kErrorOperationFailed},
    {"permission_denied", kErrorPermissionDenied},
    {"disconnected", kErrorDisconnected},
    {"expired_token", kErrorExpiredToken},
    {"invalid_token", kErrorInvalidToken},
    {"maxretries", kErrorMaxRetries},
    {"overriddenbyset", kErrorOverriddenBySet},
    {"unavailable", kErrorUnavailable},
    {"network_error", kErrorNetworkError},
    {"write_canceled", kErrorWriteCanceled},
};

Error PersistentConnection::StatusStringToErrorCode(const std::string& status) {
  for (auto& error : g_error_codes) {
    if (status == error.error_string) {
      return error.error_code;
    }
  }
  return kErrorUnknownError;
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
