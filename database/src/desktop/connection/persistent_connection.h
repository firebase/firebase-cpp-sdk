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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_PERSISTENT_CONNECTION_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_PERSISTENT_CONNECTION_H_

#include <cassert>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "app/memory/atomic.h"
#include "app/memory/shared_ptr.h"
#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/optional.h"
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

// This is an error that only occurs internally and is not propagated to the
// user, so it is not included in the normal user-visible enum.
extern Error kErrorDataStale;

namespace internal {
namespace connection {

class PersistentConnectionEventHandler;

// Firebase server sends a response message for every request from client  in a
// format of:
//   {"b":{"d":"Error message detail","s":"ok/error_code"},"r": request_id}
// The caller should pass in a Response to PersistentConnection if it cares
// cares about the result.  Or nullptr Response if not.
// The callback will be triggered from scheduler thread when the message is
// received.
// The caller can use a class derived from Response in order to capture
// additional data to be used during the callback.  For example,
// WriteResponse can capture the Future to complete and the data
// structure reference to update.
class Response {
 public:
  typedef void (*ResponseCallback)(const SharedPtr<Response>& response);
  explicit Response(ResponseCallback callback) : callback_(callback) {
    // Do not enforce non-null callback here since the Response can be used
    // to purely capture data when request is created, ex. SendAuthResponse.
    // TODO(b/77552340): Replace the implementation of Response with
    // std::function
  }
  virtual ~Response() {}

  // Check if there is any error in response
  bool HasError() const { return error_code_ != kErrorNone; }

  // Get error code from response message. Empty if received "ok"
  const Error& GetErrorCode() const { return error_code_; }

  // Get error message from response message. Usually in human readable format.
  const std::string& GetErrorMessage() const { return error_message_; }

 protected:
  // Callback to be triggered when response message is received.
  ResponseCallback callback_;

  // Error code in the response message if it is not "ok",
  Error error_code_;

  // Error message in the response message
  std::string error_message_;

  // Friend PersistentConnection so that it can set the error code and error
  // message
  friend class PersistentConnection;
};
// Use shared point so that it is easier to be forwarded to callbacks.
typedef SharedPtr<Response> ResponsePtr;

class PersistentConnection : public ConnectionEventHandler {
 public:
  explicit PersistentConnection(App* app, const HostInfo& info,
                                PersistentConnectionEventHandler* event_handler,
                                scheduler::Scheduler* scheduler,
                                Logger* logger);
  ~PersistentConnection();

  // PersistentConnection is neither copyable nor movable.
  PersistentConnection(const PersistentConnection&) = delete;
  PersistentConnection& operator=(const PersistentConnection&) = delete;

  // Begin ConnectionEventHandler
  void OnCacheHost(const std::string& host) override;
  void OnReady(int64_t timestamp, const std::string& session_id) override;
  void OnDataMessage(const Variant& message) override;
  void OnDisconnect(Connection::DisconnectReason reason) override;
  void OnKill(const std::string& reason) override;
  // End ConnectionEventHandler

  // Schedule to initialize the connection.  Can be called in any thread
  void ScheduleInitialize();

  // Schedule to shutdown the connection.  Can be called in any thread
  // Once shutdown, this PersistentConnection cannot be used again.
  void ScheduleShutdown();

  // Request to listen with a given query spec, which contains path and query
  // params.
  // tag is required if the query filters any child data.
  // This should only be called from scheduler thread.
  // TODO(chkuang): Implement ListenHashProvider.
  void Listen(const QuerySpec& query_spec, const Tag& tag,
              ResponsePtr response);

  // Request to unlisten with a given query spec, which contains path and query
  // params.
  // This should only be called from scheduler thread.
  void Unlisten(const QuerySpec& query_spec);

  // Overwrite the value at the given path.
  // This should only be called from scheduler thread.
  void Put(const Path& path, const Variant& data, ResponsePtr response);

  // Overwrite the value at the given path.  The server compares the
  // current value using the hash.  If not the same, the server returns error
  // code "datastale".
  // This should only be called from scheduler thread.
  void CompareAndPut(const Path& path, const Variant& data,
                     const std::string& hash, ResponsePtr response);

  // Merge the value at the given path.
  // This should only be called from scheduler thread.
  void Merge(const Path& path, const Variant& data, ResponsePtr response);

  // Purge all outstanding Put/Merge/OnDisconnectPut/OnDisconnectMerge requests.
  // All response callback will be triggered with "write_canceled" error code.
  // This should only be called from scheduler thread.
  void PurgeOutstandingWrites();

  // Overwrite the value at the given path on disconnection.
  // This should only be called from scheduler thread.
  void OnDisconnectPut(const Path& path, const Variant& data,
                       ResponsePtr response);

  // Merge the value at the given path on disconnection.
  // This should only be called from scheduler thread.
  void OnDisconnectMerge(const Path& path, const Variant& updates,
                         ResponsePtr response);

  // Cancel all OnDisconnect operation at the given path.
  // This should only be called from scheduler thread.
  void OnDisconnectCancel(const Path& path, ResponsePtr response);

  // Disconnect to server manually.  This is used when Database::GoOffline()
  // This should only be called from scheduler thread.
  void Interrupt();

  // Connect to server manually.  This is used when Database::GoOnline()
  // Note that the connection may be established immediately if there is any
  // other interupt reason, ex. token refresh.
  // This should only be called from scheduler thread.
  void Resume();

  // Check if the connection is currently interrupted manually.
  // This should only be called from scheduler thread.
  bool IsInterrupted();

 private:
  // Enum of all the reason to interrupt the connection.
  // There can be multiple reason to interrupt.  Only when all reason is
  // removed will the connection be resumed.
  // TODO(chkuang): kInterruptIdle and kInterruptTokenRefresh to be added
  enum InterruptReason {
    kInterruptManual = 0,
    kInterruptServerKill,
    kInterruptShutdown,
  };

  // Enum of current state of connection.
  // TODO(chkuang): kGettingToken and kAuthenticating is not actually supported
  //                but is here as a place holder
  enum ConnectionState {
    kDisconnected = 0,
    kGettingToken,
    kConnecting,
    kAuthenticating,
    kConnected
  };

  // The callback function to be triggered first when a response message is
  // received.  This should be a class function pointer.
  typedef void (PersistentConnection::*ConnectionResponseHandler)(
      const Variant& message, const ResponsePtr& response,
      uint64_t outstanding_id);

  // Request Data containing the Response and a callback.
  // The callback is responsible to trigger the callback in Response.
  // Also, the callback to be used to pre-process data before triggering
  // Response, or when Response is irrelevant (ex. ConnectionStatus message)
  struct RequestData {
    explicit RequestData(ResponsePtr response, ConnectionResponseHandler cb,
                         uint64_t id)
        : response(Move(response)), callback(cb), outstanding_id(id) {}

    // Pointer to the response.  Can be nullptr
    ResponsePtr response;

    // Callback to be triggered first when a corresponding response message is
    // received from the server
    ConnectionResponseHandler callback;

    // Custom id to find outstanding puts or listens
    uint64_t outstanding_id;
  };
  typedef UniquePtr<RequestData> RequestDataPtr;

  // Capture the outstanding or ongoing listen requests.
  struct OutstandingListen {
    explicit OutstandingListen(const QuerySpec& query_spec, const Tag& tag,
                               ResponsePtr response, uint64_t outstanding_id)
        : query_spec(query_spec),
          tag(tag),
          response(response),
          outstanding_id(outstanding_id) {}

    // Path and query params for the listen request.
    QuerySpec query_spec;

    // Tag is required if the query spec filters any children.
    Tag tag;

    // Response pointer to be triggered once the response is received
    ResponsePtr response;

    // This is used to search for QuerySpec when the client receives the
    // response message from server.  This is a solution to avoid using
    // std::function with lambda capture in RequestData.
    uint64_t outstanding_id;
  };
  typedef UniquePtr<OutstandingListen> OutstandingListenPtr;

  // Capture the outstanding OnDisconnect requests when the connection is not
  // established yet.
  struct OutstandingOnDisconnect {
    explicit OutstandingOnDisconnect(const char* action, const Path& path,
                                     const Variant& data, ResponsePtr response)
        : action(action), path(path), data(data), response(Move(response)) {}

    // Action of the request such as PUT, MERGE and CANCEL
    std::string action;

    // Database path
    Path path;

    // Data to be used for the action.  Null for CANCEL action
    Variant data;

    // Response pointer to be triggered once the response is received
    ResponsePtr response;
  };
  typedef UniquePtr<OutstandingOnDisconnect> OutstandingOnDisconnectPtr;

  // Capture the outstanding Put requests when the connection is not
  // established yet.
  struct OutstandingPut {
    explicit OutstandingPut(const char* action, const Variant& data,
                            ResponsePtr response)
        : action(action), data(data), response(Move(response)), sent(false) {}

    // Action of the request such as PUT, MERGE and CANCEL
    std::string action;

    // Data to be used for the action.  Null for CANCEL action
    Variant data;

    // Response pointer to be triggered once the response is received
    ResponsePtr response;

    // Whether the put request is sent or not
    bool sent;

    void MarkSent() { sent = true; }

    bool WasSent() { return sent; }
  };
  typedef UniquePtr<OutstandingPut> OutstandingPutPtr;

  void InterruptInternal(InterruptReason reason);
  void ResumeInternal(InterruptReason reason);
  bool IsInterruptedInternal(InterruptReason reason) {
    return interrupt_reasons_.find(reason) != interrupt_reasons_.end();
  }

  bool ShouldReconnect() { return interrupt_reasons_.empty(); }

  // Try to reconnect to RT DB server.
  void TryScheduleReconnect();

  // Callback function when the future to fetch token is complete.
  static void OnTokenFutureComplete(const Future<std::string>& result_data,
                                    void* user_data);
  void HandleTokenFuture(Future<std::string> future);

  // Pending future to fetch the token.
  Future<std::string> pending_token_future_;
  // Mutex to protect pending_token_future_
  Mutex pending_token_future_mutex_;

  // Start to establish connection to RT DB server.
  void OpenNetworkConnection();

  bool CanSendWrites() const { return connection_state_ == kConnected; }

  bool IsConnected() const {
    return connection_state_ == kConnected ||
           connection_state_ == kAuthenticating;
  }

  void HandleConnectStatsResponse(const Variant& message,
                                  const ResponsePtr& response,
                                  uint64_t outstanding_id);

  // Send listen request through websockets.
  void SendListen(const OutstandingListen& listen);

  // Callback function triggered when the server response to listen request is
  // received.
  void HandleListenResponse(const Variant& message, const ResponsePtr& response,
                            uint64_t listen_id);

  // Log warnings from the server response to listen request.
  void WarnOnListenerWarnings(const Variant& warnings,
                              const QuerySpec& query_spec);

  // Send unlisten request to the server.
  void SendUnlisten(const OutstandingListen& listen);

  // Remove the outstanding listen requests given query spec.
  OutstandingListenPtr RemoveListen(const QuerySpec& query_spec);

  // Process the server async action messages, ex. data update and auth revoke.
  void OnDataPush(const std::string& action, const Variant& body);

  // Remove the listen and manufacture a "permission denied" error for the
  // failed listen.
  void OnListenRevoked(const Path& path);

  void PutInternal(const char* action, const Path& path, const Variant& data,
                   const char* hash, ResponsePtr response);

  void SendPut(uint64_t write_id);

  void HandlePutResponse(const Variant& message, const ResponsePtr& response,
                         uint64_t outstanding_id);

  void CancelSentTransactions();

  void SendOnDisconnect(const char* action, const Path& path,
                        const Variant& data, ResponsePtr response);
  void HandleOnDisconnectResponse(const Variant& message,
                                  const ResponsePtr& response,
                                  uint64_t outstanding_id);

  void SendSensitive(const char* action, bool sensitive, const Variant& message,
                     ResponsePtr response, ConnectionResponseHandler callback,
                     uint64_t outstanding_id);

  // Restore outstanding requests created when connection is not established,
  // or before auth token is accepted by the server after the connection is
  // established.
  void RestoreOutstandingRequests();

  // Get auth token synchronously.
  void GetAuthToken(std::string* out);

  // Check if auth token is changed each time a new request is created.
  void CheckAuthTokenAndSendOnChange();

  // Send auth token message to firebase server.
  // If this is sent from OnReady(), need to restore outstanding requests once
  // the server accepts the token, i.e. restore_outstanding_on_response should
  // be true.  Otherwise, if it is sent when token is refreshed when connected,
  // no need to restore outstanding requests.
  void SendAuthToken(const std::string& token,
                     bool restore_outstanding_on_response);

  // Send unauth message to server after logout
  void SendUnauth();

  // ConnectionResponseHandler function for SendAuthToken request.
  void HandleAuthTokenResponse(const Variant& message,
                               const ResponsePtr& response,
                               uint64_t outstanding_id);

  void OnAuthRevoked(Error error_code, const std::string& reason);

  static void TriggerResponse(const ResponsePtr& response_ptr, Error error_code,
                              const std::string& error_message);

  static Error StatusStringToErrorCode(const std::string& status);

  // Wire protocol data message keys and values
  static const char* kRequestError;
  static const char* kRequestQueries;
  static const char* kRequestTag;
  static const char* kRequestStatus;
  static const char* kRequestStatusOk;
  static const char* kRequestPath;
  static const char* kRequestNumber;
  static const char* kRequestPayload;
  static const char* kRequestCounters;
  static const char* kRequestDataPayload;
  static const char* kRequestDataHash;
  static const char* kRequestCompoundHash;
  static const char* kRequestCompoundHashPaths;
  static const char* kRequestCompoundHashHashes;
  static const char* kRequestCredential;
  static const char* kRequestAuthVar;
  static const char* kRequestAction;
  static const char* kRequestActionStats;
  static const char* kRequestActionQuery;
  static const char* kRequestActionPut;
  static const char* kRequestActionMerge;
  static const char* kRequestActionQueryUnlisten;
  static const char* kRequestActionOnDisconnectPut;
  static const char* kRequestActionOnDisconnectMerge;
  static const char* kRequestActionOnDisconnectCancel;
  static const char* kRequestActionAuth;
  static const char* kRequestActionGauth;
  static const char* kRequestActionUnauth;
  static const char* kRequestNoAuth;
  static const char* kResponseForRequest;
  static const char* kServerAsyncAction;
  static const char* kServerAsyncPayload;
  static const char* kServerAsyncDataUpdate;
  static const char* kServerAsyncDataMerge;
  static const char* kServerAsyncDataRangeMerge;
  static const char* kServerAsyncAuthRevoked;
  static const char* kServerAsyncListenCancelled;
  static const char* kServerAsyncSecurityDebug;
  static const char* kServerDataUpdatePath;
  static const char* kServerDataUpdateBody;
  static const char* kServerDataStartPath;
  static const char* kServerDataEndPath;
  static const char* kServerDataRangeMerge;
  static const char* kServerDataTag;
  static const char* kServerDataWarnings;
  static const char* kServerResponseData;

  static int kInvalidAuthTokenThreshold;

  // Log id.  Unique for each persistent connection.
  static compat::Atomic<uint32_t> next_log_id_;
  std::string log_id_;

  // Reference to Firebase App.  Primarily used to get auth token.
  ::firebase::App* app_;

  // Safe reference to this.  Set in constructor and cleared in destructor
  // Should be safe to be copied in any thread because the SharedPtr never
  // changes, until safe_this_ is completely destroyed.
  typedef firebase::internal::SafeReference<PersistentConnection> ThisRef;
  typedef firebase::internal::SafeReferenceLock<PersistentConnection>
      ThisRefLock;
  ThisRef safe_this_;

  // Scheduler to make sure all Connection events are handled in worker
  // thread.
  scheduler::Scheduler* scheduler_;

  // Host info for websocket url
  const HostInfo host_info_;

  PersistentConnectionEventHandler* event_handler_;

  // Current connection.
  UniquePtr<Connection> realtime_;

  // States
  ConnectionState connection_state_;
  bool is_first_connection_;

  // Session
  std::string last_session_id_;

  // Number of time when auth token is rejected by the server
  int invalid_auth_token_count;

  // Request/Response
  uint64_t next_request_id_;
  std::map<uint64_t, RequestDataPtr> request_map_;

  // Auth
  std::string auth_token_;
  bool force_auth_refresh_;

  // Interrupt
  std::set<InterruptReason> interrupt_reasons_;

  // Outstanding listen requests
  std::map<QuerySpec, OutstandingListenPtr> listens_;

  // Map from listen_id to QuerySpec.
  // This is to capture QuerySpec per listen request for later to lookup
  // the OutstandingListen when the response is received. Ideally, this can
  // be solved by upgrading RequestData::ConnectionResponseHandler to
  // std::function and use lambda capture, or additional generic payload field
  // in RequestData that will be properly deleted if the response is never
  // received when the application is closed, to prevent memory leaking. We use
  // map for now ro simplify the solution until we investigate b/123547556.
  // TODO(chkuang): Investigate if we can just use lambda capture.
  std::map<uint64_t, QuerySpec> listen_id_to_query_;

  // Next listen id for listen requests.  Should only be accessed in scheduler_
  // thread.
  uint64_t next_listen_id_;

  // OnDisconnect
  std::queue<OutstandingOnDisconnectPtr> outstanding_ondisconnects_;

  // Outstanding puts
  std::map<uint64_t, OutstandingPutPtr> outstanding_puts_;

  // Next write id for put requests
  uint64_t next_write_id_;

  Logger* logger_;
};

class PersistentConnectionEventHandler {
 public:
  virtual ~PersistentConnectionEventHandler() {}

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

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_PERSISTENT_CONNECTION_H_
