#include "firestore/src/ios/credentials_provider_ios.h"

#include <string>
#include <utility>

#include "firestore/src/ios/hard_assert_ios.h"
#include "firebase/firestore/firestore_errors.h"
#include "Firestore/core/src/util/status.h"

namespace firebase {
namespace firestore {
namespace {

using auth::CredentialChangeListener;
using auth::Token;
using auth::TokenListener;
using auth::User;
using ::firebase::auth::Auth;
using util::Status;
using util::StatusOr;

User GetCurrentUser(Auth* firebase_auth) {
  ::firebase::auth::User* user = firebase_auth->current_user();
  if (user != nullptr) {
    return User(user->uid());
  }

  return User();
}

StatusOr<Token> ConvertToken(const Future<std::string>& future,
                             Auth* firebase_auth) {
  if (future.error() != Error::kErrorOk) {
    // `AuthError` is a different error domain from go/canonical-codes that
    // `Status` uses, so it can't be converted directly. Instead, use
    // `kErrorUnknown` in the `Status` because the error code from the future
    // is "from a different error domain".
    // TODO(b/174485290) Map `AuthError` values to Firestore `Error` values more
    // intelligently so as to enable retries when appropriate.
    return Status(Error::kErrorUnknown,
                  std::string(future.error_message()) + " (AuthError " +
                      std::to_string(future.error()) + ")");
  }

  return Token(*future.result(), GetCurrentUser(firebase_auth));
}

// Converts the result of the given future into an `firestore::auth::Token`
// and invokes the `listener` with the token. If the future is failed, invokes
// the `listener` with the error. If the current token generation is higher
// than `expected_generation`, will invoke the `listener` with "aborted"
// error. `future_token` must be a completed future.
void OnToken(const Future<std::string>& future_token, Auth* firebase_auth,
             int token_generation, const TokenListener& listener,
             int expected_generation) {
  HARD_ASSERT_IOS(future_token.status() == FutureStatus::kFutureStatusComplete,
                  "Expected to receive a completed future");

  if (expected_generation != token_generation) {
    // Cancel the request since the user may have changed while the request was
    // outstanding, so the response is likely for a previous user (which user,
    // we can't be sure).
    listener(Status(Error::kErrorAborted,
                    "GetToken() aborted due to token change."));
    return;
  }

  listener(ConvertToken(future_token, firebase_auth));
}

}  // namespace

FirebaseCppCredentialsProvider::FirebaseCppCredentialsProvider(
    Auth* firebase_auth)
    : contents_(std::make_shared<Contents>(NOT_NULL(firebase_auth))) {}

void FirebaseCppCredentialsProvider::SetCredentialChangeListener(
    CredentialChangeListener listener) {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);

  if (!listener) {
    HARD_ASSERT_IOS(change_listener_,
                    "Change listener removed without being set!");
    change_listener_ = {};
    // Note: not removing the Auth listener here because the Auth might already
    // be destroyed. Note that Auth listeners unregister themselves upon
    // destruction anyway.
    return;
  }

  HARD_ASSERT_IOS(!change_listener_, "Set change listener twice!");
  change_listener_ = std::move(listener);
  change_listener_(GetCurrentUser(contents_->firebase_auth));

  // Note: make sure to only register the Auth listener _after_ calling
  // `Auth::current_user` for the first time. The reason is that upon the only
  // first call only, `Auth::current_user` might block as it would
  // asynchronously notify Auth listeners; getting the Firestore listener
  // notified while `Auth::current_user` is pending can lead to a deadlock.
  contents_->firebase_auth->AddAuthStateListener(this);
}

void FirebaseCppCredentialsProvider::GetToken(TokenListener listener) {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);

  if (IsSignedIn()) {
    RequestToken(std::move(listener));
  } else {
    listener(Token::Unauthenticated());
  }
}

void FirebaseCppCredentialsProvider::InvalidateToken() {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
  force_refresh_token_ = true;
}

void FirebaseCppCredentialsProvider::OnAuthStateChanged(Auth* firebase_auth) {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);

  // The currently signed-in user may have changed, increase the token
  // generation to reflect that.
  ++(contents_->token_generation);

  if (change_listener_) {
    change_listener_(GetCurrentUser(contents_->firebase_auth));
  }
}

// Private member functions

void FirebaseCppCredentialsProvider::RequestToken(TokenListener listener) {
  HARD_ASSERT_IOS(IsSignedIn(),
                  "Cannot get token when there is no signed-in user");

  bool force_refresh = force_refresh_token_;
  force_refresh_token_ = false;
  // Take note of the current value of `token_generation` so that this request
  // can fail if there is a token change while the request is outstanding.
  int expected_generation = contents_->token_generation;

  auto future =
      contents_->firebase_auth->current_user()->GetToken(force_refresh);
  std::weak_ptr<Contents> weak_contents(contents_);
  // Note: if the future happens to be already completed (either because the
  // token was readily available, or theoretically because the Auth token
  // request finished so quickly), this completion will be invoked
  // synchronously. Because the mutex is recursive, it's okay to lock it again
  // in that case.
  future.OnCompletion(
      // TODO(c++14): move `listener` into the lambda.
      [listener, expected_generation,
       weak_contents](const Future<std::string>& future_token) {
        auto contents = weak_contents.lock();
        // Auth may invoke the callback when credentials provider has already
        // been destroyed.
        if (!contents) {
          return;
        }

        std::lock_guard<std::recursive_mutex> lock(contents->mutex);
        OnToken(future_token, contents->firebase_auth,
                contents->token_generation, std::move(listener),
                expected_generation);
      });
}

bool FirebaseCppCredentialsProvider::IsSignedIn() const {
  return contents_->firebase_auth->current_user() != nullptr;
}

}  // namespace firestore
}  // namespace firebase
