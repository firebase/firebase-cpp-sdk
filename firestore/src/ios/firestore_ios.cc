#include "firestore/src/ios/firestore_ios.h"

#include <utility>

#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "auth/src/include/firebase/auth.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/create_firebase_metadata_provider.h"
#include "firestore/src/ios/credentials_provider_ios.h"
#include "firestore/src/ios/document_reference_ios.h"
#include "firestore/src/ios/document_snapshot_ios.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "firestore/src/ios/listener_ios.h"
#include "absl/memory/memory.h"
#include "absl/types/any.h"
#include "firebase/firestore/firestore_version.h"
#include "Firestore/core/src/api/document_reference.h"
#include "Firestore/core/src/api/query_core.h"
#include "Firestore/core/src/model/database_id.h"
#include "Firestore/core/src/model/resource_path.h"
#include "Firestore/core/src/util/async_queue.h"
#include "Firestore/core/src/util/executor.h"
#include "Firestore/core/src/util/log.h"
#include "Firestore/core/src/util/status.h"

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

std::shared_ptr<AsyncQueue> CreateWorkerQueue() {
  auto executor = Executor::CreateSerial("com.google.firebase.firestore");
  return AsyncQueue::Create(std::move(executor));
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
      firestore_core_(CreateFirestore(app, std::move(credentials))),
      transaction_executor_(absl::ShareUniquePtr(Executor::CreateConcurrent(
          "com.google.firebase.firestore.transaction", /*threads=*/5))) {
  ApplyDefaultSettings();

  App::RegisterLibrary("fire-fst", kFirestoreVersionString);
}

FirestoreInternal::~FirestoreInternal() {
  ClearListeners();
  transaction_executor_->Dispose();
  firestore_core_->Dispose();
}

std::shared_ptr<api::Firestore> FirestoreInternal::CreateFirestore(
    App* app, std::unique_ptr<CredentialsProvider> credentials) {
  const AppOptions& opt = app->options();
  return std::make_shared<api::Firestore>(
      DatabaseId{opt.project_id()}, app->name(), std::move(credentials),
      CreateWorkerQueue(),
      CreateFirebaseMetadataProvider(*app), this);
}

CollectionReference FirestoreInternal::Collection(
    const char* collection_path) const {
  auto result = firestore_core_->GetCollection(collection_path);
  return MakePublic(std::move(result));
}

DocumentReference FirestoreInternal::Document(const char* document_path) const {
  auto result = firestore_core_->GetDocument(document_path);
  return MakePublic(std::move(result));
}

Query FirestoreInternal::CollectionGroup(const char* collection_id) const {
  core::Query core_query = firestore_core_->GetCollectionGroup(collection_id);
  api::Query api_query(std::move(core_query), firestore_core_);
  return MakePublic(std::move(api_query));
}

Settings FirestoreInternal::settings() const {
  static_assert(
      Settings::kDefaultCacheSizeBytes == api::Settings::DefaultCacheSizeBytes,
      "kDefaultCacheSizeBytes must be in sync between the public API and the "
      "core API");
  static_assert(
      Settings::kCacheSizeUnlimited == api::Settings::CacheSizeUnlimited,
      "kCacheSizeUnlimited must be in sync between the public API and the core "
      "API");

  Settings result;

  const api::Settings& from = firestore_core_->settings();
  result.set_host(from.host());
  result.set_ssl_enabled(from.ssl_enabled());
  result.set_persistence_enabled(from.persistence_enabled());
  result.set_cache_size_bytes(from.cache_size_bytes());

  return result;
}

void FirestoreInternal::set_settings(Settings from) {
  api::Settings settings;
  settings.set_host(std::move(from.host()));
  settings.set_ssl_enabled(from.is_ssl_enabled());
  settings.set_persistence_enabled(from.is_persistence_enabled());
  settings.set_cache_size_bytes(from.cache_size_bytes());
  firestore_core_->set_settings(settings);

  std::unique_ptr<Executor> user_executor = from.CreateExecutor();
  firestore_core_->set_user_executor(std::move(user_executor));
}

WriteBatch FirestoreInternal::batch() const {
  return MakePublic(firestore_core_->GetBatch());
}

Future<void> FirestoreInternal::RunTransaction(TransactionFunction* update) {
  return RunTransaction(
      [update](Transaction& transaction, std::string& error_message) {
        return update->Apply(transaction, error_message);
      });
}

Future<void> FirestoreInternal::RunTransaction(
    std::function<Error(Transaction&, std::string&)> update) {
  auto executor = transaction_executor_;
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

              Error error_code = update(transaction, error_message);
              if (error_code == Error::kErrorOk) {
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

  firestore_core_->RunTransaction(std::move(update_callback),
                                  std::move(final_result_callback));

  return promise.future();
}

Future<void> FirestoreInternal::DisableNetwork() {
  auto promise =
      promise_factory_.CreatePromise<void>(AsyncApi::kDisableNetwork);
  firestore_core_->DisableNetwork(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::EnableNetwork() {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApi::kEnableNetwork);
  firestore_core_->EnableNetwork(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::Terminate() {
  auto promise = promise_factory_.CreatePromise<void>(AsyncApi::kTerminate);
  ClearListeners();
  firestore_core_->Terminate(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::WaitForPendingWrites() {
  auto promise =
      promise_factory_.CreatePromise<void>(AsyncApi::kWaitForPendingWrites);
  firestore_core_->WaitForPendingWrites(StatusCallbackWithPromise(promise));
  return promise.future();
}

Future<void> FirestoreInternal::ClearPersistence() {
  auto promise =
      promise_factory_.CreatePromise<void>(AsyncApi::kClearPersistence);
  firestore_core_->ClearPersistence(StatusCallbackWithPromise(promise));
  return promise.future();
}

void FirestoreInternal::ClearListeners() {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  for (auto* listener : listeners_) {
    listener->Remove();
    delete listener;
  }
  listeners_.clear();
}

ListenerRegistration FirestoreInternal::AddSnapshotsInSyncListener(
    EventListener<void>* listener) {
  std::function<void()> listener_function = [listener] {
    listener->OnEvent(Error::kErrorOk, EmptyString());
  };
  auto result = firestore_core_->AddSnapshotsInSyncListener(
      ListenerWithCallback(std::move(listener_function)));
  return MakePublic(std::move(result), this);
}

ListenerRegistration FirestoreInternal::AddSnapshotsInSyncListener(
    std::function<void()> callback) {
  auto result = firestore_core_->AddSnapshotsInSyncListener(
      ListenerWithCallback(std::move(callback)));
  return MakePublic(std::move(result), this);
}

void FirestoreInternal::RegisterListenerRegistration(
    ListenerRegistrationInternal* registration) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  listeners_.insert(registration);
}

void FirestoreInternal::UnregisterListenerRegistration(
    ListenerRegistrationInternal* registration) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  auto iter = listeners_.find(registration);
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

void Firestore::set_log_level(LogLevel log_level) {
  switch (log_level) {
    case kLogLevelVerbose:
    case kLogLevelDebug:
      // Firestore doesn't have the distinction between "verbose" and "debug".
      LogSetLevel(util::kLogLevelDebug);
      break;
    case kLogLevelInfo:
      LogSetLevel(util::kLogLevelNotice);
      break;
    case kLogLevelWarning:
      LogSetLevel(util::kLogLevelWarning);
      break;
    case kLogLevelError:
    case kLogLevelAssert:
      // Firestore doesn't have a separate "assert" log level.
      LogSetLevel(util::kLogLevelError);
      break;
    default:
      FIRESTORE_UNREACHABLE();
      break;
  }

  // Call SetLogLevel() to keep the C++ log level in sync with FIRLogger's.
  // Convert kLogLevelDebug to kLogLevelVerbose to force debug logs to be
  // emitted. See b/159048318 for details.
  firebase::SetLogLevel(log_level == kLogLevelDebug ? kLogLevelVerbose
                                                    : log_level);
}

void FirestoreInternal::SetClientLanguage(const std::string& language_token) {
  api::Firestore::SetClientLanguage(language_token);
}

}  // namespace firestore
}  // namespace firebase
