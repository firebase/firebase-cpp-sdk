#include "absl/memory/memory.h"
#include "auth/src/include/firebase/auth.h"
#include "firestore/src/ios/create_credentials_provider.h"
#include "firestore/src/ios/credentials_provider_desktop.h"

namespace firebase {
namespace firestore {

using auth::CredentialsProvider;
using firebase::auth::Auth;

std::unique_ptr<CredentialsProvider> CreateCredentialsProvider(App& app) {
  return absl::make_unique<FirebaseCppCredentialsProvider>(app);
}

}  // namespace firestore
}  // namespace firebase
