#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CREDENTIALS_PROVIDER_DESKTOP_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CREDENTIALS_PROVIDER_DESKTOP_H_

#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <string>

#include "Firestore/core/src/auth/credentials_provider.h"
#include "Firestore/core/src/auth/token.h"
#include "Firestore/core/src/auth/user.h"
#include "Firestore/core/src/util/statusor.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"

namespace firebase {
namespace firestore {

// Glues together C++ Firebase Auth and Firestore: allows Firestore to listen to
// Auth events and to retrieve auth tokens. Thread-safe.
//
// This is a language-specific implementation of `CredentialsProvider` that
// works with the public C++ Auth.
class FirebaseCppCredentialsProvider
    : public firestore::auth::CredentialsProvider {
 public:
  explicit FirebaseCppCredentialsProvider(App& app);
  ~FirebaseCppCredentialsProvider() override;

  FirebaseCppCredentialsProvider(const FirebaseCppCredentialsProvider&) =
      delete;
  FirebaseCppCredentialsProvider& operator=(
      const FirebaseCppCredentialsProvider&) = delete;

  // `firestore::auth::CredentialsProvider` interface.
  void SetCredentialChangeListener(
      firestore::auth::CredentialChangeListener listener) override;
  void GetToken(firestore::auth::TokenListener listener) override;
  void InvalidateToken() override;

 private:
  void AddAuthStateListener();
  void RemoveAuthStateListener();

  // Callback for the function registry-based pseudo-AuthStateListener
  // interface.
  static void OnAuthStateChanged(void* context);

  // Requests an auth token for the currently signed-in user asynchronously; the
  // given `listener` will eventually be invoked with the token (or an error).
  // If there is no signed-in user, immediately invokes the `listener` with
  // `Token::Unauthenticated()`.
  void RequestToken(firestore::auth::TokenListener listener);

  bool IsSignedIn() const;

  // Wraps the data that is used by `auth::User::GetToken` callback. This
  // credentials provider holds a shared pointer to `Contents`, while the
  // `GetToken` callback stores a weak pointer. This makes safe the case where
  // the `GetToken` callback might be invoked after this credentials provider
  // has already been destroyed (Auth may outlive Firestore).
  struct Contents {
    explicit Contents(App& app) : app(app) {}

    // FirebaseCppCredentialsProvider may be used by more than one thread. The
    // mutex is locked in all public member functions and none of the private
    // member functions (with the exception of `RequestToken` that locks the
    // mutex in a lambda that gets involved asynchronously later). Therefore,
    // the invariant is that when a private member function is involved, the
    // mutex is always already locked. The mutex is recursive to avoid one
    // potential case of deadlock (attaching a continuation to a `Future` which
    // may be invoked immediately or asynchronously).
    //
    // TODO(b/148688333): make sure not to hold the mutex while calling methods
    // on `app`.
    std::recursive_mutex mutex;

    App& app;

    // Each time credentials change, the token "generation" is incremented.
    // Credentials commonly change when a different user signs in; comparing
    // generations at the point where a token is requested and the point where
    // the token is retrieved allows identifying obsolete requests.
    int token_generation = 0;
  };
  std::shared_ptr<Contents> contents_;

  // Affects the next `GetToken` request; if `true`, the token will be refreshed
  // even if it hasn't expired yet.
  bool force_refresh_token_ = false;

  // Provided by the user code; may be an empty function.
  firestore::auth::CredentialChangeListener change_listener_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_CREDENTIALS_PROVIDER_DESKTOP_H_
