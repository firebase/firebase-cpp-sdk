// Copyright 2021 Google LLC
#include "firestore/src/main/credentials_provider_desktop.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "Firestore/core/src/util/status.h"
#include "app/src/function_registry.h"
#include "app/src/reference_counted_future_impl.h"
#include "auth/src/include/firebase/auth/types.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"

namespace firebase::firestore {
namespace {

std::mutex& ListenerMutex() {
  static auto* mutex = new std::mutex();
  return *mutex;
}

std::unordered_map<uintptr_t, std::weak_ptr<FirebaseCppCredentialsProvider::Contents>>&
ValidContexts() {
  static auto* contexts =
      new std::unordered_map<uintptr_t, std::weak_ptr<FirebaseCppCredentialsProvider::Contents>>();
  return *contexts;
}

uintptr_t GetNextListenerId() {
  static std::atomic<uintptr_t> next_id{1};
  uintptr_t id = next_id.fetch_add(1);
  return id == 0 ? next_id.fetch_add(1) : id;
}

using credentials::AuthToken;
using credentials::CredentialChangeListener;
using credentials::TokenListener;
using credentials::User;
using firebase::auth::AuthError;
using util::Status;
using util::StatusOr;

Error FirestoreErrorFromAuthError(int error) {
  switch (error) {
    case AuthError::kAuthErrorNone: return kErrorNone;
    case AuthError::kAuthErrorUnimplemented: return kErrorUnimplemented;
    case AuthError::kAuthErrorFailure: return kErrorInternal;
    case AuthError::kAuthErrorNetworkRequestFailed: return kErrorUnavailable;
    case AuthError::kAuthErrorCancelled: return kErrorCancelled;
    case AuthError::kAuthErrorInvalidCustomToken:
    case AuthError::kAuthErrorInvalidCredential:
    case AuthError::kAuthErrorUserDisabled:
    case AuthError::kAuthErrorUserNotFound:
    case AuthError::kAuthErrorInvalidUserToken:
    case AuthError::kAuthErrorUserTokenExpired:
    case AuthError::kAuthErrorNoSignedInUser:
      return kErrorUnauthenticated;
    default:
      return kErrorUnknown;
  }
}

Future<std::string> GetAuthTokenAsync(App& app, bool force_refresh) {
  Future<std::string> result;
  if (app.function_registry()->CallFunction(
          ::firebase::internal::FnAuthGetTokenAsync, &app, &force_refresh, &result)) {
    return result;
  }
  return SuccessfulFuture<std::string>("");
}

User GetCurrentUser(App& app) {
  std::string uid;
  if (!app.function_registry()->CallFunction(
          ::firebase::internal::FnAuthGetCurrentUserUid, &app, nullptr, &uid)) {
    return {};
  }
  return User(uid);
}

StatusOr<AuthToken> ConvertToken(const Future<std::string>& future, const User& user) {
  if (future.error() != Error::kErrorOk) {
    const char* msg = future.error_message();
    return Status(FirestoreErrorFromAuthError(future.error()),
                  std::string(msg ? msg : "") + " (AuthError " +
                      std::to_string(future.error()) + ")");
  }
  const std::string* token = future.result();
  return token ? AuthToken(*token, user) : AuthToken::Unauthenticated();
}

void OnToken(const Future<std::string>& future_token, App& app, int token_generation,
             const TokenListener<AuthToken>& listener, int expected_generation) {
  SIMPLE_HARD_ASSERT(future_token.status() == FutureStatus::kFutureStatusComplete,
                     "Expected to receive a completed future");
  if (expected_generation != token_generation) {
    listener(Status(Error::kErrorAborted, "GetToken() aborted due to token change."));
    return;
  }
  listener(ConvertToken(future_token, GetCurrentUser(app)));
}

}  // namespace

FirebaseCppCredentialsProvider::FirebaseCppCredentialsProvider(App& app)
    : contents_(std::make_shared<Contents>(app)) {}

FirebaseCppCredentialsProvider::~FirebaseCppCredentialsProvider() {
  {
    std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
    contents_->auth_listener = {};
  }
  RemoveAuthStateListener();
}

void FirebaseCppCredentialsProvider::SetCredentialChangeListener(
    CredentialChangeListener<User> listener) {
  if (!listener) {
    {
      std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
      SIMPLE_HARD_ASSERT(contents_->auth_listener, "Change listener removed without being set!");
      contents_->auth_listener = {};
    }
    RemoveAuthStateListener();
    return;
  }
  {
    std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
    SIMPLE_HARD_ASSERT(!contents_->auth_listener, "Set change listener twice!");
    contents_->auth_listener = listener;
    listener(GetCurrentUser(contents_->app));
  }
  AddAuthStateListener();
}

void FirebaseCppCredentialsProvider::GetToken(TokenListener<AuthToken> listener) {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
  RequestToken(std::move(listener));
}

void FirebaseCppCredentialsProvider::InvalidateToken() {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
  force_refresh_ = true;
}

void FirebaseCppCredentialsProvider::AddAuthStateListener() {
  App* app = nullptr;
  void* context = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
    if (!contents_->auth_listener || auth_listener_context_) return;
    app = &contents_->app;
    uintptr_t listener_id = GetNextListenerId();
    context = reinterpret_cast<void*>(listener_id);
    auth_listener_context_ = context;
    std::lock_guard<std::mutex> listener_lock(ListenerMutex());
    ValidContexts()[listener_id] = std::weak_ptr<Contents>(contents_);
  }
  app->function_registry()->CallFunction(
      ::firebase::internal::FnAuthAddAuthStateListener, app,
      reinterpret_cast<void*>(OnAuthStateChanged), context);
}

void FirebaseCppCredentialsProvider::RemoveAuthStateListener() {
  App* app = nullptr;
  void* context = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
    if (!auth_listener_context_) return;
    app = &contents_->app;
    context = std::exchange(auth_listener_context_, nullptr);
    std::lock_guard<std::mutex> listener_lock(ListenerMutex());
    ValidContexts().erase(reinterpret_cast<uintptr_t>(context));
  }
  app->function_registry()->CallFunction(
      ::firebase::internal::FnAuthRemoveAuthStateListener, app,
      reinterpret_cast<void*>(OnAuthStateChanged), context);
}

void FirebaseCppCredentialsProvider::OnAuthStateChanged(void* context) {
  uintptr_t listener_id = reinterpret_cast<uintptr_t>(context);
  std::shared_ptr<Contents> contents;
  {
    std::lock_guard<std::mutex> lock(ListenerMutex());
    auto it = ValidContexts().find(listener_id);
    if (it == ValidContexts().end()) return;
    contents = it->second.lock();
  }
  if (!contents) return;

  std::lock_guard<std::recursive_mutex> lock(contents->mutex);
  {
    std::lock_guard<std::mutex> listener_lock(ListenerMutex());
    if (ValidContexts().find(listener_id) == ValidContexts().end()) return;
  }
  ++(contents->token_generation);
  if (auto listener = contents->auth_listener) {
    listener(GetCurrentUser(contents->app));
  }
}

void FirebaseCppCredentialsProvider::RequestToken(TokenListener<AuthToken> listener) {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
  if (!IsSignedIn()) {
    listener(AuthToken::Unauthenticated());
    return;
  }
  int expected_generation = contents_->token_generation;
  bool force_refresh = force_refresh_.exchange(false);
  auto future = GetAuthTokenAsync(contents_->app, force_refresh);
  if (future.status() == FutureStatus::kFutureStatusInvalid) {
    listener(AuthToken::Unauthenticated());
    return;
  }
  std::weak_ptr<Contents> weak_contents(contents_);
  future.OnCompletion(
      [listener = std::move(listener), expected_generation,
       weak_contents](const Future<std::string>& future_token) mutable {
        auto contents = weak_contents.lock();
        if (!contents) {
          if (listener) {
            listener(Status(Error::kErrorAborted, "Credentials provider was destroyed."));
          }
          return;
        }
        std::lock_guard<std::recursive_mutex> lock(contents->mutex);
        OnToken(future_token, contents->app, contents->token_generation, listener,
                expected_generation);
      });
}

bool FirebaseCppCredentialsProvider::IsSignedIn() const {
  return GetCurrentUser(contents_->app).is_authenticated();
}

}  // namespace firebase::firestore
