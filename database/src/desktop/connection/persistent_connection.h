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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_PERSISTENT_CONNECTION_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_PERSISTENT_CONNECTION_H_

#include <stdint.h>

#include <atomic>
#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/connection/connection.h"
#include "database/src/desktop/connection/host_info.h"
#include "database/src/desktop/core/tag.h"
#include "database/src/include/firebase/database/common.h"

namespace firebase {
namespace database {

extern Error kErrorDataStale;

namespace internal {
namespace connection {

class PersistentConnectionEventHandler;

class Response {
 public:
  typedef void (*ResponseCallback)(const std::shared_ptr<Response>& response);
  explicit Response(ResponseCallback callback) : callback_(callback) {}
  virtual ~Response() = default;

  bool HasError() const { return error_code_ != kErrorNone; }
  const Error& GetErrorCode() const { return error_code_; }
  const std::string& GetErrorMessage() const { return error_message_; }

 protected:
  ResponseCallback callback_;
  Error error_code_ = kErrorNone;
  std::string error_message_;
  friend class PersistentConnection;
};
typedef std::shared_ptr<Response> ResponsePtr;

class PersistentConnection : public ConnectionEventHandler {
 public:
  explicit PersistentConnection(App* app, const HostInfo& info,
                                PersistentConnectionEventHandler* event_handler,
                                scheduler::Scheduler* scheduler,
                                Logger* logger);
  ~PersistentConnection() override;

  PersistentConnection(const PersistentConnection&) = delete;
  PersistentConnection& operator=(const PersistentConnection&) = delete;

  void OnCacheHost(const std::string& host) override;
  void OnReady(int64_t timestamp, const std::string& session_id) override;
  void OnDataMessage(const Variant& message) override;
  void OnDisconnect(Connection::DisconnectReason reason) override;
  void OnKill(const std::string& reason) override;

  void ScheduleInitialize();
  void ScheduleShutdown();

  void Listen(const QuerySpec& query_spec, const Tag& tag,
              ResponsePtr response);
  void Unlisten(const QuerySpec& query_spec);
  void Put(const Path& path, const Variant& data, ResponsePtr response);
  void CompareAndPut(const Path& path, const Variant& data,
                     const std::string& hash, ResponsePtr response);
  void Merge(const Path& path, const Variant& data, ResponsePtr response);
  void PurgeOutstandingWrites(Error error = kErrorWriteCanceled);

  void OnDisconnectPut(const Path& path, const Variant& data,
                       ResponsePtr response);
  void OnDisconnectMerge(const Path& path, const Variant& updates,
                         ResponsePtr response);
  void OnDisconnectCancel(const Path& path, ResponsePtr response);

  void Interrupt() { InterruptInternal(kInterruptManual); }
  void Resume() { ResumeInternal(kInterruptManual); }
  bool IsInterrupted() { return IsInterruptedInternal(kInterruptManual); }
  void RefreshAppCheckToken(const std::string& token);

 private:
  enum InterruptReason {
    kInterruptManual = 0,
    kInterruptServerKill,
    kInterruptShutdown,
  };
  enum ConnectionState {
    kDisconnected = 0,
    kGettingToken,
    kConnecting,
    kAuthenticating,
    kConnected
  };

  typedef void (PersistentConnection::*ConnectionResponseHandler)(
      const Variant& message, const ResponsePtr& response,
      uint64_t outstanding_id);

  struct RequestData {
    RequestData(ResponsePtr resp, ConnectionResponseHandler cb, uint64_t id)
        : response(std::move(resp)), callback(cb), outstanding_id(id) {}
    ResponsePtr response;
    ConnectionResponseHandler callback;
    uint64_t outstanding_id;
  };
  typedef std::unique_ptr<RequestData> RequestDataPtr;

  struct OutstandingListen {
    OutstandingListen(const QuerySpec& spec, const Tag& t, ResponsePtr resp,
                      uint64_t id)
        : query_spec(spec),
          tag(t),
          response(std::move(resp)),
          outstanding_id(id) {}
    QuerySpec query_spec;
    Tag tag;
    ResponsePtr response;
    uint64_t outstanding_id;
  };
  typedef std::unique_ptr<OutstandingListen> OutstandingListenPtr;

  struct OutstandingOnDisconnect {
    OutstandingOnDisconnect(const char* act, const Path& p, const Variant& d,
                            ResponsePtr resp)
        : action(act), path(p), data(d), response(std::move(resp)) {}
    std::string action;
    Path path;
    Variant data;
    ResponsePtr response;
  };
  typedef std::unique_ptr<OutstandingOnDisconnect> OutstandingOnDisconnectPtr;

  struct OutstandingPut {
    OutstandingPut(const char* act, const Variant& d, ResponsePtr resp)
        : action(act), data(d), response(std::move(resp)), sent(false) {}
    std::string action;
    Variant data;
    ResponsePtr response;
    bool sent;
    void MarkSent() { sent = true; }
    bool WasSent() const { return sent; }
  };
  typedef std::unique_ptr<OutstandingPut> OutstandingPutPtr;

  void InterruptInternal(InterruptReason reason);
  void ResumeInternal(InterruptReason reason);
  bool IsInterruptedInternal(InterruptReason reason) {
    return interrupt_reasons_.find(reason) != interrupt_reasons_.end();
  }
  bool ShouldReconnect() const { return interrupt_reasons_.empty(); }
  void TryScheduleReconnect();

  static void OnAuthTokenFutureComplete(const Future<std::string>& auth_future,
                                        void* user_data);
  static void OnAppCheckTokenFutureComplete(
      const Future<std::string>& app_check_future, void* user_data);
  void HandleTokenFutures();

  Mutex pending_token_future_mutex_;
  enum TokenFutureStatus {
    kInvalidTokenFuture = 0,
    kWaitingForTokenFuture,
    kCompletedTokenFuture,
  };
  Future<std::string> pending_auth_token_future_;
  TokenFutureStatus auth_token_future_status_ = kInvalidTokenFuture;
  Future<std::string> pending_app_check_token_future_;
  TokenFutureStatus app_check_token_future_status_ = kInvalidTokenFuture;

  void OpenNetworkConnection();
  bool CanSendWrites() const { return connection_state_ == kConnected; }
  bool IsConnected() const {
    return connection_state_ == kConnected ||
           connection_state_ == kAuthenticating;
  }

  void HandleConnectStatsResponse(const Variant& message,
                                  const ResponsePtr& response,
                                  uint64_t outstanding_id);
  void SendListen(const OutstandingListen& listen);
  void HandleListenResponse(const Variant& message, const ResponsePtr& response,
                            uint64_t listen_id);
  void WarnOnListenerWarnings(const Variant& warnings,
                              const QuerySpec& query_spec);
  void SendUnlisten(const OutstandingListen& listen);
  OutstandingListenPtr RemoveListen(const QuerySpec& query_spec);
  void OnDataPush(const std::string& action, const Variant& body);
  void OnListenRevoked(const Path& path);

  void PutInternal(const char* action, const Path& path, const Variant& data,
                   const char* hash, ResponsePtr response);
  void SendPut(uint64_t write_id);
  void HandlePutResponse(const Variant& message, const ResponsePtr& response,
                         uint64_t outstanding_id);
  void CancelSentTransactions();

  void QueueOrSendOnDisconnect(const char* action, const Path& path,
                               const Variant& data, ResponsePtr response);
  void SendOnDisconnect(const char* action, const Path& path,
                        const Variant& data, ResponsePtr response);
  void HandleOnDisconnectResponse(const Variant& message,
                                  const ResponsePtr& response,
                                  uint64_t outstanding_id);
  void SendSensitive(const char* action, bool sensitive, const Variant& message,
                     ResponsePtr response, ConnectionResponseHandler callback,
                     uint64_t outstanding_id);
  void RestoreOutstandingRequests();
  void GetAuthToken(std::string* out);
  void CheckAuthTokenAndSendOnChange();
  void SendAuthToken(const std::string& token,
                     bool restore_outstanding_on_response);
  void SendUnauth();
  void HandleAuthTokenResponse(const Variant& message,
                               const ResponsePtr& response,
                               uint64_t outstanding_id);
  void OnAuthRevoked(Error error_code, const std::string& reason);
  void HandleAppCheckTokenResponse(const Variant& message,
                                   const ResponsePtr& response,
                                   uint64_t outstanding_id);

  static void TriggerResponse(const ResponsePtr& response_ptr, Error error_code,
                              const std::string& error_message);
  static Error StatusStringToErrorCode(const std::string& status);

  static std::atomic<uint32_t> next_log_id_;
  std::string log_id_;
  ::firebase::App* app_;

  typedef firebase::internal::SafeReference<PersistentConnection> ThisRef;
  typedef firebase::internal::SafeReferenceLock<PersistentConnection>
      ThisRefLock;
  ThisRef safe_this_;
  scheduler::Scheduler* scheduler_;
  const HostInfo host_info_;
  PersistentConnectionEventHandler* event_handler_;
  std::unique_ptr<Connection> realtime_;

  ConnectionState connection_state_ = kDisconnected;
  bool is_first_connection_ = true;
  std::string last_session_id_;
  int invalid_auth_token_count = 0;

  uint64_t next_request_id_ = 0;
  std::map<uint64_t, RequestDataPtr> request_map_;

  std::string auth_token_;
  bool force_auth_refresh_ = false;
  std::string app_check_token_;
  std::set<InterruptReason> interrupt_reasons_;

  std::map<QuerySpec, OutstandingListenPtr> listens_;
  std::map<uint64_t, QuerySpec> listen_id_to_query_;
  uint64_t next_listen_id_ = 0;

  std::queue<OutstandingOnDisconnectPtr> outstanding_ondisconnects_;
  std::map<uint64_t, OutstandingPutPtr> outstanding_puts_;
  uint64_t next_write_id_ = 0;
  Logger* logger_;
};

class PersistentConnectionEventHandler {
 public:
  virtual ~PersistentConnectionEventHandler() = default;
  virtual void OnConnect() = 0;
  virtual void OnDisconnect() = 0;
  virtual void OnAuthStatus(bool auth_ok) = 0;
  virtual void OnServerInfoUpdate(
      const std::map<Variant, Variant>& updates) = 0;
  virtual void OnDataUpdate(const Path& path, const Variant& payload_data,
                            bool is_merge, const Tag& tag) = 0;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_PERSISTENT_CONNECTION_H_
