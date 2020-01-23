#include "firestore/src/ios/firestore_ios.h"

#include <utility>

#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "auth/src/include/firebase/auth.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/credentials_provider_ios.h"
#include "firestore/src/ios/document_reference_ios.h"
#include "firestore/src/ios/document_snapshot_ios.h"
#include "firestore/src/ios/listener_ios.h"
#include "absl/memory/memory.h"
#include "absl/types/any.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/api/document_reference.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/api/query_core.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/model/database_id.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/model/resource_path.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/util/async_queue.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/util/executor.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/util/log.h"
#include "firebase-ios-sdk/Firestore/core/src/firebase/firestore/util/status.h"

namespace firebase {
namespace firestore {

namespace {

using auth::CredentialsProvider;
using ::firebase::auth::Auth;
using model::DatabaseId;
using model::ResourcePath;
using util::AsyncQueue;
using util::Executor;
using util::Status;

std::unique_ptr<AsyncQueue> CreateWorkerQueue() {
  auto executor = Executor::CreateSerial("com.google.firebase.firestore");
  return absl::make_unique<AsyncQueue>(std::move(executor));
}

std::unique_ptr<CredentialsProvider> CreateCredentialsProvider(App* app) {
  return absl::make_unique<FirebaseCppCredentialsProvider>(Auth::GetAuth(app));
}

}  // namespace

FirestoreInternal::FirestoreInternal(App* app)
    : FirestoreInternal{app, CreateCredentialsProvider(app)} {}

FirestoreInternal::FirestoreInternal(
    App* app, std::unique_ptr<CredentialsProvider> credentials)
    : app_(NOT_NULL(app)),
      firestore_(CreateFirestore(app, std::move(credentials))) {
  ApplyDefaultSettings();
}

FirestoreInternal::~FirestoreInternal() { ClearListeners(); }

std::shared_ptr<api::Firestore> FirestoreInternal::CreateFirestore(
    App* app, std::unique_ptr<CredentialsProvider> credentials) {
  const AppOptions& opt = app->options();
  return std::make_shared<api::Firestore>(DatabaseId{opt.project_id()},
                                          app->name(), std::move(credentials),
                                          CreateWorkerQueue(), this);
}

CollectionReference FirestoreInternal::Collection(
    const char* collection_path) const {
  auto result = firestore_->GetCollection(collection_path);
  return MakePublic(std::move(result));
}

DocumentReference FirestoreInternal::Document(const char* document_path) const {
  auto result = firestore_->GetDocument(document_path);
  return MakePublic(std::move(result));
}

Query FirestoreInternal::CollectionGroup(const char* collection_id) const {
  core::Query core_query = firestore_->GetCollectionGroup(collection_id);
  api::Query api_query(std::move(core_query), firestore_);
  return MakePublic(std::move(api_query));
}

Settings FirestoreInternal::settings() const {
  Settings result;

  const api::Settings& from = firestore_->settings();
  result.set_host(from.host());
  result.set_ssl_enabled(from.ssl_enabled());
  result.set_persistence_enabled(from.persistence_enabled());
  // TODO(varconst): implement `cache_size_bytes` in public `Settings` and
  // uncomment.
  // result.set_cache_size_bytes(from.cache_size_bytes());

  return result;
}

void FirestoreInternal::set_settings(const Settings& from) {
  api::Settings settings;
  settings.set_host(from.host());
  settings.set_ssl_enabled(from.is_ssl_enabled());
  settings.set_persistence_enabled(from.is_persistence_enabled());
  // TODO(varconst): implement `cache_size_bytes` in public `Settings` and
  // uncomment.
  // settings.set_cache_size_bytes(from.cache_size_bytes());
  firestore_->set_settings(settings);

  std::unique_ptr<Executor> user_executor = from.CreateExecutor();
  firestore_->set_user_executor(std::move(user_executor));
}

WriteBatch FirestoreInternal::batch() const {
  return MakePublic(firestore_->GetBatch());
}

Future<void> FirestoreInternal::RunTransaction(TransactionFunction* update) {
  return RunTransaction(
      [update](Transaction* transaction, std::string* error_message) {
        return update->Apply(transaction, error_message);
      });
}

Future<void> FirestoreInternal::RunTransaction(
    std::function<Error(Transaction*, std::string*)> update) {
  auto executor = absl::ShareUniquePtr(Executor::CreateConcurrent(
      "com.google.firebase.firestore.transaction", /*threads=*/5));
  auto promise =
      promise_factory_.CreatePromise<void>(AsyncApi::kRunTransaction);

  // TODO(c++14): move into lambda.
  auto update_callback =
      [this, update, executor](
          std::shared_ptr<core::Transaction> core_transaction,
          core::TransactionResultCallback eventual_result_callback) {
        executor->Execute(
            [this, update, core_transaction, eventual_result_callback] {
              std::string error_message;

              // Note: there is no `MakePublic` overload for `Transaction`
              // because it is not copyable or movable and thus cannot be
              // returned from a function.
              auto* transaction_internal =
                  new TransactionInternal(core_transaction, this);
              Transaction transaction{transaction_internal};

              Error error_code = update(&transaction, &error_message);
              if (error_code == Error::Ok) {
                eventual_result_callback(Status::OK());
              } else {
                // TODO(varconst): port this from iOS
                // If the error is a user error, set flag to not retry the
                // transaction.
                // if (is_user_error) {
                //   transaction.MarkPermanentlyFailed();
                // }
                eventual_result_callback(Status{error_code, error_message});
              }
            });
      };

  auto final_result_callback = [promise](util::Status status) mutable {
    if (status.ok()) {
      // Note: the result is deliberately ignored here, because it is not
      // clear how to surface the `any` to the public API.
      promise.SetValue();
    } else {
      promise.SetError(std::move(status));
    }
  };

  firestore_->RunTransaction(std::move(update_callback),
                             std::move(final_result_callback));

  return promise.future();
}

Future<void> FirestoreInternal::RunTransactionLastResult() {
  return promise_factory_.LastResult<void>(AsyncApi::kRunTransaction);
}

Future<void> FirestoreInternal::DisableNetwork() {
  auto promise =
      promise_factory_.CreatePromise<void>(AsyncApi::kDisableNetwork);
  firestore_->DisableNetwork(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::DisableNetworkLastResult() {
  return promise_factory_.LastResult<void>(AsyncApi::kDisableNetwork);
}

Future<void> FirestoreInternal::EnableNetwork() {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApi::kEnableNetwork);
  firestore_->EnableNetwork(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::EnableNetworkLastResult() {
  return promise_factory_.LastResult<void>(AsyncApi::kEnableNetwork);
}

Future<void> FirestoreInternal::Terminate() {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApi::kTerminate);
  ClearListeners();
  firestore_->Terminate(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::TerminateLastResult() {
  return promise_factory_.LastResult<void>(AsyncApi::kTerminate);
}

Future<void> FirestoreInternal::WaitForPendingWrites() {
  auto promise =
      promise_factory_.CreatePromise<void>(AsyncApi::kWaitForPendingWrites);
  firestore_->WaitForPendingWrites(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::WaitForPendingWritesLastResult() {
  return promise_factory_.LastResult<void>(AsyncApi::kWaitForPendingWrites);
}

Future<void> FirestoreInternal::ClearPersistence() {
  auto promise =
      promise_factory_.CreatePromise<void>(AsyncApi::kClearPersistence);
  firestore_->ClearPersistence(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::ClearPersistenceLastResult() {
  return promise_factory_.LastResult<void>(AsyncApi::kClearPersistence);
}

void FirestoreInternal::ClearListeners() {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  for (auto* listener : listeners_) {
    listener->Remove();
  }
  listeners_.clear();
}

void FirestoreInternal::RegisterListenerRegistration(
    ListenerRegistrationInternal* listener) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  listeners_.insert(listener);
}

void FirestoreInternal::UnregisterListenerRegistration(
    ListenerRegistrationInternal* listener) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  auto iter = listeners_.find(listener);
  if (iter != listeners_.end()) {
    delete *iter;
    listeners_.erase(iter);
  }
}

void FirestoreInternal::ApplyDefaultSettings() {
  // Explicitly apply the default settings to the underlying `api::Firestore`,
  // because otherwise, its executor will stay null (unless the user happens to
  // call `set_settings`, which we cannot rely upon).
  set_settings(settings());
}

void Firestore::set_logging_enabled(bool logging_enabled) {
  LogSetLevel(logging_enabled ? util::kLogLevelDebug : util::kLogLevelNotice);
}

}  // namespace firestore
}  // namespace firebase
