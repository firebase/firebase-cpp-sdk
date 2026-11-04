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
#include <cassert>
#include <cstdint>
#include <sstream>
#include <utility>

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

Error kErrorDataStale = static_cast<Error>(-1);

namespace internal {
namespace connection {

namespace {
constexpr const char* kRequestError = "error";
constexpr const char* kRequestQueries = "q";
constexpr const char* kRequestTag = "t";
constexpr const char* kRequestStatus = "s";
constexpr const char* kRequestStatusOk = "ok";
constexpr const char* kRequestPath = "p";
constexpr const char* kRequestNumber = "r";
constexpr const char* kRequestPayload = "b";
constexpr const char* kRequestCounters = "c";
constexpr const char* kRequestDataPayload = "d";
constexpr const char* kRequestDataHash = "h";
constexpr const char* kRequestCredential = "cred";
constexpr const char* kRequestAction = "a";
constexpr const char* kRequestActionStats = "s";
constexpr const char* kRequestActionQuery = "q";
constexpr const char* kRequestActionPut = "p";
constexpr const char* kRequestActionMerge = "m";
constexpr const char* kRequestActionQueryUnlisten = "n";
constexpr const char* kRequestActionOnDisconnectPut = "o";
constexpr const char* kRequestActionOnDisconnectMerge = "om";
constexpr const char* kRequestActionOnDisconnectCancel = "oc";
constexpr const char* kRequestActionAuth = "auth";
constexpr const char* kRequestActionUnauth = "unauth";
constexpr const char* kResponseForRequest = "b";
constexpr const char* kServerAsyncAction = "a";
constexpr const char* kServerAsyncPayload = "b";
constexpr const char* kServerAsyncDataUpdate = "d";
constexpr const char* kServerAsyncDataMerge = "m";
constexpr const char* kServerAsyncDataRangeMerge = "rm";
constexpr const char* kServerAsyncAuthRevoked = "ac";
constexpr const char* kServerAsyncListenCancelled = "c";
constexpr const char* kServerAsyncSecurityDebug = "sd";
constexpr const char* kServerDataUpdatePath = "p";
constexpr const char* kServerDataUpdateBody = "d";
constexpr const char* kServerDataTag = "t";
constexpr const char* kServerDataWarnings = "w";
constexpr const char* kServerResponseData = "d";
constexpr int kInvalidAuthTokenThreshold = 3;

std::string GetDebugQuerySpecString(const QuerySpec& query_spec) {
  return WireProtocolPathToString(query_spec.path) + " params: " +
         util::VariantToJson(GetWireProtocolParams(query_spec.params)) + ")";
}

std::string GetStringValue(const Variant& data, const char* key,
                           bool force = false) {
  if (!data.is_map()) return "";
  auto it = data.map().find(key);
  if (it == data.map().end()) return "";
  if (it->second.is_string()) return it->second.string_value();
  return force ? util::VariantToJson(it->second) : "";
}

bool HasKey(const Variant& data, const char* key) {
  return data.is_map() && data.map().find(key) != data.map().end();
}

Variant BuildListenRequest(const QuerySpec& spec, const Tag& tag) {
  Variant request = Variant::EmptyMap();
  auto& map = request.map();
  map[kRequestPath] = WireProtocolPathToString(spec.path);
  if (tag.has_value()) {
    map[kRequestQueries] = GetWireProtocolParams(spec.params);
    map[kRequestTag] = tag.value();
  }
  return request;
}

class SendAuthResponse : public Response {
 public:
  explicit SendAuthResponse(bool restore) : Response(nullptr), restore_(restore) {}
  bool GetRestoreOutstandingsFlag() const { return restore_; }

 private:
  bool restore_;
};
}  // namespace

std::atomic<uint32_t> PersistentConnection::next_log_id_(0);

PersistentConnection::PersistentConnection(
    App* app, const HostInfo& info,
    PersistentConnectionEventHandler* event_handler,
    scheduler::Scheduler* scheduler, Logger* logger)
    : app_(app),
      safe_this_(this),
      scheduler_(scheduler),
      host_info_(info),
      event_handler_(event_handler),
      logger_(logger) {
  FIREBASE_DEV_ASSERT(app);
  FIREBASE_DEV_ASSERT(scheduler);
  FIREBASE_DEV_ASSERT(event_handler_);
  log_id_ = "[pc_" + std::to_string(next_log_id_.fetch_add(1)) + "]";
}

PersistentConnection::~PersistentConnection() {
  safe_this_.ClearReference();
  {
    MutexLock future_lock(pending_token_future_mutex_);
    pending_auth_token_future_ = Future<std::string>();
    auth_token_future_status_ = kInvalidTokenFuture;
    pending_app_check_token_future_ = Future<std::string>();
    app_check_token_future_status_ = kInvalidTokenFuture;
  }
  realtime_.reset();
}

void PersistentConnection::OnCacheHost(const std::string& /*host*/) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);
}

void PersistentConnection::OnReady(int64_t timestamp,
                                   const std::string& session_id) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);
  logger_->LogDebug("%s OnReady", log_id_.c_str());
  logger_->LogDebug("%s Handle timestamp: %lld in ms", log_id_.c_str(),
                    timestamp);
  int64_t time_delta = timestamp - ::firebase::internal::GetTimestampEpoch();
  event_handler_->OnServerInfoUpdate(
      {{kDotInfoServerTimeOffset, time_delta}});

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

  logger_->LogDebug("%s calling restore state", log_id_.c_str());
  FIREBASE_DEV_ASSERT(connection_state_ == kConnecting);
  GetAuthToken(&auth_token_);

  if (auth_token_.empty()) {
    logger_->LogDebug("%s Not restoring auth because token is null.",
                      log_id_.c_str());
    connection_state_ = kConnected;
    RestoreOutstandingRequests();
  } else {
    logger_->LogDebug("%s Restoring auth", log_id_.c_str());
    connection_state_ = kAuthenticating;
    SendAuthToken(auth_token_, true);
  }

  last_session_id_ = session_id;
  event_handler_->OnConnect();
}

void PersistentConnection::HandleConnectStatsResponse(
    const Variant& message, const ResponsePtr& /*response*/,
    uint64_t /*outstanding_id*/) {
  auto status = GetStringValue(message, kRequestStatus);
  if (status != kRequestStatusOk && GetLogLevel() > kLogLevelInfo) {
    logger_->LogDebug(
        "%s Failed to send stats: %s  (message: %s)", log_id_.c_str(),
        status.c_str(),
        GetStringValue(message, kServerDataUpdateBody, true).c_str());
  }
}

void PersistentConnection::OnDataMessage(const Variant& message) {
  FIREBASE_DEV_ASSERT(message.is_map());
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);

  if (HasKey(message, kRequestNumber)) {
    auto it_rn = message.map().find(kRequestNumber);
    FIREBASE_DEV_ASSERT(it_rn->second.is_numeric());
    uint64_t rn = it_rn->second.int64_value();

    RequestDataPtr request_ptr;
    auto it_req = request_map_.find(rn);
    FIREBASE_DEV_ASSERT(it_req != request_map_.end());
    if (it_req != request_map_.end()) {
      request_ptr = std::move(it_req->second);
      request_map_.erase(it_req);
    }
    FIREBASE_DEV_ASSERT(request_ptr);
    if (request_ptr) {
      auto it_resp = message.map().find(kResponseForRequest);
      FIREBASE_DEV_ASSERT(it_resp != message.map().end());
      if (it_resp != message.map().end()) {
        logger_->LogDebug("%s Trigger handler for request %llu",
                          log_id_.c_str(), rn);
        if (request_ptr->callback) {
          (*this.*request_ptr->callback)(it_resp->second, request_ptr->response,
                                         request_ptr->outstanding_id);
        }
      }
    }
  } else if (HasKey(message, kRequestError)) {
    logger_->LogError("%s Received Error Data Message: %s", log_id_.c_str(),
                      GetStringValue(message, kRequestError, true).c_str());
  } else if (HasKey(message, kServerAsyncAction)) {
    auto* action = GetInternalVariant(&message, kServerAsyncAction);
    if (!action || !action->is_string()) {
      logger_->LogError("Received Server Async Action is not a string.");
    }
    auto* body = GetInternalVariant(&message, kServerAsyncPayload);
    if (action && body) OnDataPush(action->string_value(), *body);
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
  realtime_.reset();
  request_map_.clear();
  CancelSentTransactions();
  if (ShouldReconnect()) TryScheduleReconnect();
  event_handler_->OnDisconnect();
}

void PersistentConnection::OnKill(const std::string& reason) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);
  logger_->LogDebug(
      "%s Firebase Database connection was forcefully killed by the server. "
      "Will not attempt reconnect. Reason: %s",
      log_id_.c_str(), reason.c_str());
  InterruptInternal(kInterruptServerKill);
  PurgeOutstandingWrites(kErrorDisconnected);
}

void PersistentConnection::ScheduleInitialize() {
  scheduler_->Schedule(
      new callback::CallbackValue1<ThisRef>(safe_this_, [](ThisRef ref) {
        ThisRefLock lock(&ref);
        if (auto* conn = lock.GetReference()) conn->TryScheduleReconnect();
      }));
}

void PersistentConnection::ScheduleShutdown() {
  scheduler_->Schedule(
      new callback::CallbackValue1<ThisRef>(safe_this_, [](ThisRef ref) {
        ThisRefLock lock(&ref);
        if (auto* conn = lock.GetReference()) {
          conn->InterruptInternal(kInterruptShutdown);
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
  uint64_t listen_id = next_listen_id_++;
  auto [it, _] = listens_.emplace(
      query_spec, std::make_unique<OutstandingListen>(
                      query_spec, tag, std::move(response), listen_id));
  listen_id_to_query_[listen_id] = query_spec;
  if (IsConnected()) SendListen(*it->second);
}

void PersistentConnection::Unlisten(const QuerySpec& query_spec) {
  CheckAuthTokenAndSendOnChange();
  logger_->LogDebug("%s Unlisten on %s", log_id_.c_str(),
                    GetDebugQuerySpecString(query_spec).c_str());
  OutstandingListenPtr listen = RemoveListen(query_spec);
  if (listen && IsConnected()) SendUnlisten(*listen);
}

void PersistentConnection::Put(const Path& path, const Variant& data,
                               ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  PutInternal(kRequestActionPut, path, data, nullptr, std::move(response));
}

void PersistentConnection::CompareAndPut(const Path& path, const Variant& data,
                                         const std::string& hash,
                                         ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  PutInternal(kRequestActionPut, path, data, hash.c_str(), std::move(response));
}

void PersistentConnection::Merge(const Path& path, const Variant& data,
                                 ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  PutInternal(kRequestActionMerge, path, data, nullptr, std::move(response));
}

void PersistentConnection::PurgeOutstandingWrites(Error error) {
  for (auto& [id, put] : outstanding_puts_) {
    TriggerResponse(put->response, error, GetErrorMessage(error));
  }
  outstanding_puts_.clear();
  while (!outstanding_ondisconnects_.empty()) {
    TriggerResponse(outstanding_ondisconnects_.front()->response, error,
                    GetErrorMessage(error));
    outstanding_ondisconnects_.pop();
  }
}

void PersistentConnection::QueueOrSendOnDisconnect(const char* action,
                                                   const Path& path,
                                                   const Variant& data,
                                                   ResponsePtr response) {
  CheckAuthTokenAndSendOnChange();
  if (CanSendWrites()) {
    SendOnDisconnect(action, path, data, std::move(response));
  } else {
    outstanding_ondisconnects_.push(std::make_unique<OutstandingOnDisconnect>(
        action, path, data, std::move(response)));
  }
}

void PersistentConnection::OnDisconnectPut(const Path& path,
                                           const Variant& data,
                                           ResponsePtr response) {
  QueueOrSendOnDisconnect(kRequestActionOnDisconnectPut, path, data,
                          std::move(response));
}

void PersistentConnection::OnDisconnectMerge(const Path& path,
                                             const Variant& updates,
                                             ResponsePtr response) {
  QueueOrSendOnDisconnect(kRequestActionOnDisconnectMerge, path, updates,
                          std::move(response));
}

void PersistentConnection::OnDisconnectCancel(const Path& path,
                                              ResponsePtr response) {
  QueueOrSendOnDisconnect(kRequestActionOnDisconnectCancel, path,
                          Variant::Null(), std::move(response));
}

void PersistentConnection::InterruptInternal(InterruptReason reason) {
  logger_->LogDebug("%s Connection interrupted for: %d", log_id_.c_str(),
                    static_cast<int>(reason));
  interrupt_reasons_.insert(reason);
  if (realtime_) {
    realtime_->Close();
    realtime_.reset();
  } else {
    connection_state_ = kDisconnected;
  }
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
  if (!ShouldReconnect()) return;
  FIREBASE_DEV_ASSERT(connection_state_ == kDisconnected);
  bool force_refresh = force_auth_refresh_;
  force_auth_refresh_ = false;
  logger_->LogDebug("%s Scheduling connection attempt", log_id_.c_str());

  scheduler_->Schedule(new callback::CallbackValue2<ThisRef, bool>(
      safe_this_, force_refresh, [](ThisRef ref, bool force_refresh) {
        ThisRefLock lock(&ref);
        auto* conn = lock.GetReference();
        if (!conn) return;
        conn->connection_state_ = kGettingToken;
        conn->logger_->LogDebug("%s Trying to fetch auth token",
                                conn->log_id_.c_str());

        Future<std::string> auth_future;
        bool auth_ok = conn->app_->function_registry()->CallFunction(
            ::firebase::internal::FnAuthGetTokenAsync, conn->app_,
            &force_refresh, &auth_future);
        Future<std::string> app_check_future;
        bool ac_ok = conn->app_->function_registry()->CallFunction(
            ::firebase::internal::FnAppCheckGetTokenAsync, conn->app_, nullptr,
            &app_check_future);

        auth_ok &= auth_future.status() != kFutureStatusInvalid;
        ac_ok &= app_check_future.status() != kFutureStatusInvalid;

        if (auth_ok || ac_ok) {
          MutexLock future_lock(conn->pending_token_future_mutex_);
          conn->pending_auth_token_future_ =
              auth_ok ? auth_future : Future<std::string>();
          conn->auth_token_future_status_ =
              auth_ok ? kWaitingForTokenFuture : kInvalidTokenFuture;
          conn->pending_app_check_token_future_ =
              ac_ok ? app_check_future : Future<std::string>();
          conn->app_check_token_future_status_ =
              ac_ok ? kWaitingForTokenFuture : kInvalidTokenFuture;
          if (auth_ok) {
            auth_future.OnCompletion(OnAuthTokenFutureComplete, conn);
          }
          if (ac_ok) {
            app_check_future.OnCompletion(OnAppCheckTokenFutureComplete, conn);
          }
        } else {
          conn->auth_token_.clear();
          conn->app_check_token_.clear();
          conn->OpenNetworkConnection();
        }
      }));
}

void PersistentConnection::OnAuthTokenFutureComplete(
    const Future<std::string>& result_data, void* user_data) {
  FIREBASE_DEV_ASSERT(user_data);
  auto* conn = static_cast<PersistentConnection*>(user_data);
  ThisRefLock lock(&conn->safe_this_);
  if (!lock.GetReference()) return;
  {
    MutexLock future_lock(conn->pending_token_future_mutex_);
    if (conn->pending_auth_token_future_ != result_data) return;
    conn->auth_token_future_status_ = kCompletedTokenFuture;
    if (conn->app_check_token_future_status_ == kWaitingForTokenFuture) return;
  }
  conn->scheduler_->Schedule(new callback::CallbackValue1<ThisRef>(
      conn->safe_this_, [](ThisRef ref) {
        ThisRefLock lock(&ref);
        if (auto* c = lock.GetReference()) c->HandleTokenFutures();
      }));
}

void PersistentConnection::OnAppCheckTokenFutureComplete(
    const Future<std::string>& result_data, void* user_data) {
  FIREBASE_DEV_ASSERT(user_data);
  auto* conn = static_cast<PersistentConnection*>(user_data);
  ThisRefLock lock(&conn->safe_this_);
  if (!lock.GetReference()) return;
  {
    MutexLock future_lock(conn->pending_token_future_mutex_);
    if (conn->pending_app_check_token_future_ != result_data) return;
    conn->app_check_token_future_status_ = kCompletedTokenFuture;
    if (conn->auth_token_future_status_ == kWaitingForTokenFuture) return;
  }
  conn->scheduler_->Schedule(new callback::CallbackValue1<ThisRef>(
      conn->safe_this_, [](ThisRef ref) {
        ThisRefLock lock(&ref);
        if (auto* c = lock.GetReference()) c->HandleTokenFutures();
      }));
}

void PersistentConnection::HandleTokenFutures() {
  FIREBASE_DEV_ASSERT(auth_token_future_status_ != kWaitingForTokenFuture);
  FIREBASE_DEV_ASSERT(app_check_token_future_status_ != kWaitingForTokenFuture);

  if (auth_token_future_status_ == kCompletedTokenFuture &&
      pending_auth_token_future_.error() != 0) {
    connection_state_ = kDisconnected;
    logger_->LogDebug("%s Error fetching token: %s", log_id_.c_str(),
                      pending_auth_token_future_.error_message());
    TryScheduleReconnect();
  } else if (connection_state_ == kGettingToken) {
    logger_->LogDebug("%s Successfully fetched token, opening connection",
                      log_id_.c_str());
    auth_token_ = (auth_token_future_status_ == kCompletedTokenFuture)
                      ? *pending_auth_token_future_.result()
                      : "";
    app_check_token_ = (app_check_token_future_status_ == kCompletedTokenFuture)
                           ? *pending_app_check_token_future_.result()
                           : "";
    OpenNetworkConnection();
  } else {
    FIREBASE_DEV_ASSERT(connection_state_ == kDisconnected);
    logger_->LogDebug(
        "%s Not opening connection after token refresh, because connection was "
        "set to disconnected",
        log_id_.c_str());
  }
}

void PersistentConnection::OpenNetworkConnection() {
  FIREBASE_DEV_ASSERT(connection_state_ == kGettingToken);
  if (auth_token_.empty()) event_handler_->OnAuthStatus(false);
  connection_state_ = kConnecting;
  realtime_ = std::make_unique<Connection>(
      scheduler_, host_info_,
      last_session_id_.empty() ? nullptr : last_session_id_.c_str(), this,
      logger_, app_check_token_);
  realtime_->Open();
}

void PersistentConnection::SendListen(const OutstandingListen& listen) {
  SendSensitive(kRequestActionQuery, false,
                BuildListenRequest(listen.query_spec, listen.tag),
                listen.response, &PersistentConnection::HandleListenResponse,
                listen.outstanding_id);
}

void PersistentConnection::SendUnlisten(const OutstandingListen& listen) {
  SendSensitive(kRequestActionQueryUnlisten, false,
                BuildListenRequest(listen.query_spec, listen.tag),
                ResponsePtr(), nullptr, 0);
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
  Error error_code =
      StatusStringToErrorCode(GetStringValue(message, kRequestStatus));
  bool is_ok = error_code == kErrorNone;
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
  if (!warnings.is_vector()) return;
  if (std::find(warnings.vector().begin(), warnings.vector().end(),
                Variant("no_index")) != warnings.vector().end()) {
    Variant wire_protocol = GetWireProtocolParams(query_spec.params);
    const Variant* index_on = GetInternalVariant(&wire_protocol, Variant("i"));
    logger_->LogWarning(
        "%s Using an unspecified index. Consider adding '\".indexOn\": \"%s\"' "
        "at %s to your security and Firebase Database rules for better "
        "performance",
        log_id_.c_str(),
        index_on != nullptr && index_on->is_string() ? index_on->string_value()
                                                     : "NULL",
        WireProtocolPathToString(query_spec.path).c_str());
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
    return nullptr;
  }
  OutstandingListenPtr listen_ptr = std::move(it_listen->second);
  listens_.erase(it_listen);
  listen_id_to_query_.erase(listen_ptr->outstanding_id);
  return listen_ptr;
}

void PersistentConnection::OnDataPush(const std::string& action,
                                      const Variant& body) {
  logger_->LogDebug("%s handleServerMessage %s %s", log_id_.c_str(),
                    action.c_str(), util::VariantToJson(body).c_str());

  if (action == kServerAsyncDataUpdate || action == kServerAsyncDataMerge) {
    bool is_merge = (action == kServerAsyncDataMerge);
    auto* path_variant = GetInternalVariant(&body, kServerDataUpdatePath);
    if (!path_variant) {
      logger_->LogError("Received path from Server Async Action is missing.");
    }
    auto* payload_data = GetInternalVariant(&body, kServerDataUpdateBody);
    if (!payload_data) {
      logger_->LogError(
          "Received payload data from Server Async Action is missing.");
    }
    auto* tag_variant = GetInternalVariant(&body, kServerDataTag);

    if (is_merge && payload_data != nullptr && payload_data->is_map() &&
        payload_data->map().empty()) {
      logger_->LogDebug("%s ignoring empty merge for path %s", log_id_.c_str(),
                        path_variant->AsString().string_value());
    } else {
      event_handler_->OnDataUpdate(
          Path(path_variant->AsString().string_value()), *payload_data,
          is_merge,
          tag_variant ? Tag(tag_variant->AsInt64().int64_value()) : Tag());
    }
  } else if (action == kServerAsyncDataRangeMerge) {
    // Compound Hash not yet supported.
  } else if (action == kServerAsyncListenCancelled) {
    if (auto* path = GetInternalVariant(&body, kServerDataUpdatePath)) {
      OnListenRevoked(Path(path->AsString().string_value()));
    }
  } else if (action == kServerAsyncAuthRevoked) {
    auto* status = GetInternalVariant(&body, kRequestStatus);
    auto* reason = GetInternalVariant(&body, kServerDataUpdateBody);
    Error error_code =
        status ? StatusStringToErrorCode(status->AsString().string_value())
               : kErrorUnknownError;
    OnAuthRevoked(error_code,
                  reason ? reason->AsString().string_value() : "null");
  } else if (action == kServerAsyncSecurityDebug) {
    if (auto* msg = GetInternalVariant(&body, "msg")) {
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
  for (auto it = listens_.begin(); it != listens_.end();) {
    if (it->first.path == path) {
      responses_to_trigger.push_back(it->second->response);
      it = listens_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto& response : responses_to_trigger) {
    TriggerResponse(response, kErrorPermissionDenied,
                    GetErrorMessage(kErrorPermissionDenied));
  }
}

void PersistentConnection::PutInternal(const char* action, const Path& path,
                                       const Variant& data, const char* hash,
                                       ResponsePtr response) {
  if (IsInterruptedInternal(kInterruptServerKill)) {
    TriggerResponse(response, kErrorOperationFailed,
                    GetErrorMessage(kErrorOperationFailed));
    return;
  }
  Variant request = Variant::EmptyMap();
  request.map()[kRequestPath] = path.str();
  request.map()[kRequestDataPayload] = data;
  if (hash != nullptr) request.map()[kRequestDataHash] = std::string(hash);

  uint64_t write_id = next_write_id_++;
  outstanding_puts_[write_id] =
      std::make_unique<OutstandingPut>(action, request, std::move(response));
  if (CanSendWrites()) SendPut(write_id);
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
    logger_->LogDebug("%s %s response: %s", log_id_.c_str(),
                      it_put->second->action.c_str(),
                      util::VariantToJson(message).c_str());
    Error error_code =
        StatusStringToErrorCode(GetStringValue(message, kRequestStatus));
    TriggerResponse(response, error_code,
                    error_code == kErrorNone
                        ? ""
                        : GetStringValue(message, kServerDataUpdateBody, true));
    outstanding_puts_.erase(it_put);
  } else {
    logger_->LogDebug(
        "%s Ignore on complete for put (%llu) because it was removed already.",
        log_id_.c_str(), outstanding_id);
  }
}

void PersistentConnection::CancelSentTransactions() {
  std::vector<OutstandingPutPtr> cancelled_writes;
  for (auto it = outstanding_puts_.begin(); it != outstanding_puts_.end();) {
    if (it->second->data.map().find(kRequestDataHash) !=
            it->second->data.map().end() &&
        it->second->WasSent()) {
      cancelled_writes.push_back(std::move(it->second));
      it = outstanding_puts_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto& put : cancelled_writes) {
    TriggerResponse(put->response, kErrorDisconnected,
                    GetErrorMessage(kErrorDisconnected));
  }
}

void PersistentConnection::SendOnDisconnect(const char* action,
                                            const Path& path,
                                            const Variant& data,
                                            ResponsePtr response) {
  Variant request = Variant::EmptyMap();
  request.map()[kRequestPath] = path.str();
  request.map()[kRequestDataPayload] = data;
  SendSensitive(action, false, request, std::move(response),
                &PersistentConnection::HandleOnDisconnectResponse, 0);
}

void PersistentConnection::HandleOnDisconnectResponse(
    const Variant& message, const ResponsePtr& response,
    uint64_t /*outstanding_id*/) {
  Error error_code =
      StatusStringToErrorCode(GetStringValue(message, kRequestStatus));
  TriggerResponse(response, error_code,
                  error_code == kErrorNone
                      ? ""
                      : GetStringValue(message, kServerDataUpdateBody, true));
}

void PersistentConnection::SendSensitive(const char* action, bool sensitive,
                                         const Variant& message,
                                         ResponsePtr response,
                                         ConnectionResponseHandler callback,
                                         uint64_t outstanding_id) {
  FIREBASE_DEV_ASSERT(realtime_);
  int64_t rn = ++next_request_id_;
  Variant request = Variant::EmptyMap();
  request.map()[kRequestNumber] = rn;
  request.map()[kRequestAction] = action;
  request.map()[kRequestPayload] = message;
  realtime_->Send(request, sensitive);
  request_map_[rn] = std::make_unique<RequestData>(std::move(response),
                                                   callback, outstanding_id);
}

void PersistentConnection::RestoreOutstandingRequests() {
  FIREBASE_DEV_ASSERT(connection_state_ == kConnected);
  logger_->LogDebug("%s Restoring outstanding listens", log_id_.c_str());
  for (auto& [spec, listen] : listens_) {
    logger_->LogDebug("%s Restoring listen %s", log_id_.c_str(),
                      GetDebugQuerySpecString(listen->query_spec).c_str());
    SendListen(*listen);
  }
  for (auto& [id, put] : outstanding_puts_) SendPut(id);
  while (!outstanding_ondisconnects_.empty()) {
    auto& front = outstanding_ondisconnects_.front();
    SendOnDisconnect(front->action.c_str(), front->path, front->data,
                     std::move(front->response));
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
  if (auth_token_ != old_token && IsConnected()) {
    if (!auth_token_.empty()) {
      SendAuthToken(auth_token_, false);
    } else {
      SendUnauth();
    }
  }
}

void PersistentConnection::SendAuthToken(const std::string& token,
                                         bool restore_outstanding_on_response) {
  logger_->LogDebug("%s Sending auth token", log_id_.c_str());
  Variant request = Variant::EmptyMap();
  request.map()[kRequestCredential] = token;
  SendSensitive(
      kRequestActionAuth, true, request,
      std::make_shared<SendAuthResponse>(restore_outstanding_on_response),
      &PersistentConnection::HandleAuthTokenResponse, 0);
}

void PersistentConnection::SendUnauth() {
  logger_->LogDebug("%s Sending unauth", log_id_.c_str());
  SendSensitive(kRequestActionUnauth, false, Variant::EmptyMap(), ResponsePtr(),
                nullptr, 0);
}

void PersistentConnection::HandleAuthTokenResponse(
    const Variant& message, const ResponsePtr& response,
    uint64_t /*outstanding_id*/) {
  FIREBASE_DEV_ASSERT(response);
  connection_state_ = kConnected;
  std::string status = GetStringValue(message, kRequestStatus);

  if (status == kRequestStatusOk) {
    invalid_auth_token_count = 0;
    event_handler_->OnAuthStatus(true);
    logger_->LogDebug("%s Authentication success", log_id_.c_str());
    if (static_cast<SendAuthResponse*>(response.get())
            ->GetRestoreOutstandingsFlag()) {
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

    if (StatusStringToErrorCode(status) == kErrorInvalidToken &&
        ++invalid_auth_token_count >= kInvalidAuthTokenThreshold) {
      logger_->LogWarning(
          "Provided authentication credentials are invalid. This indicates "
          "your FirebaseApp instance was not initialized correctly. Make sure "
          "your google-services.json file has the correct firebase_url and "
          "api_key. You can re-download google-services.json from "
          "https://console.firebase.google.com/.");
    }
  }
}

void PersistentConnection::OnAuthRevoked(Error error_code,
                                         const std::string& reason) {
  logger_->LogDebug("%s Auth token revoked: %i (%s)", log_id_.c_str(),
                    error_code, reason.c_str());
  auth_token_.clear();
  force_auth_refresh_ = true;
  event_handler_->OnAuthStatus(false);
  realtime_->Close();
}

void PersistentConnection::RefreshAppCheckToken(const std::string& token) {
  scheduler_->Schedule(new callback::CallbackValue2<ThisRef, std::string>(
      safe_this_, token, [](ThisRef ref, std::string token) {
        ThisRefLock lock(&ref);
        auto* conn = lock.GetReference();
        if (!conn || !conn->IsConnected() || conn->app_check_token_ == token) {
          return;
        }
        if (token.empty()) {
          conn->app_check_token_.clear();
          return;
        }
        conn->logger_->LogDebug("%s Refreshing App Check token",
                                conn->log_id_.c_str());
        conn->app_check_token_ = token;
        conn->realtime_->RefreshAppCheckToken(token);
        Variant request = Variant::EmptyMap();
        request.map()["token"] = token;
        conn->SendSensitive(
            "appcheck", true, request, ResponsePtr(),
            &PersistentConnection::HandleAppCheckTokenResponse, 0);
      }));
}

void PersistentConnection::HandleAppCheckTokenResponse(
    const Variant& message, const ResponsePtr& /*response*/,
    uint64_t /*outstanding_id*/) {
  std::string status = GetStringValue(message, kRequestStatus);
  if (status == kRequestStatusOk) {
    logger_->LogDebug("%s Refreshing App Check token succeeded.",
                      log_id_.c_str());
  } else {
    logger_->LogWarning(
        "%s Refreshing App Check token failed: %s (%s)", log_id_.c_str(),
        status.c_str(), GetStringValue(message, kServerResponseData).c_str());
  }
}

void PersistentConnection::TriggerResponse(const ResponsePtr& response_ptr,
                                           Error error_code,
                                           const std::string& error_message) {
  if (response_ptr) {
    response_ptr->error_code_ = error_code;
    response_ptr->error_message_ = error_message;
    if (response_ptr->callback_) response_ptr->callback_(response_ptr);
  }
}

Error PersistentConnection::StatusStringToErrorCode(const std::string& status) {
  static const struct {
    const char* error_string;
    Error error_code;
  } kErrorCodes[] = {
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
  for (const auto& entry : kErrorCodes) {
    if (status == entry.error_string) return entry.error_code;
  }
  return kErrorUnknownError;
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
