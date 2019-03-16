/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "auth/src/common.h"

namespace firebase {
namespace auth {

AUTH_RESULT_FN(User, GetToken, std::string)
AUTH_RESULT_FN(User, UpdateEmail, void)
AUTH_RESULT_FN(User, UpdatePassword, void)
AUTH_RESULT_FN(User, Reauthenticate, void)
AUTH_RESULT_FN(User, ReauthenticateAndRetrieveData, SignInResult)
AUTH_RESULT_FN(User, SendEmailVerification, void)
AUTH_RESULT_FN(User, UpdateUserProfile, void)
AUTH_RESULT_FN(User, LinkWithCredential, User*)
AUTH_RESULT_FN(User, LinkAndRetrieveDataWithCredential, SignInResult)
AUTH_RESULT_FN(User, Unlink, User*)
AUTH_RESULT_FN(User, UpdatePhoneNumberCredential, User*)
AUTH_RESULT_FN(User, Reload, void)
AUTH_RESULT_FN(User, Delete, void)

#if defined(INTERNAL_EXPERIMENTAL)
// I'd like to change all the above functions to use LastResultProxy, as it
// makes multi-threaded situations more deterministic. However, LastResult
// functions are public in the C++ SDK. And even while they are
// non-deterministic in multi-threaded situations, someone might rely on their
// current behavior. So for now, we only use it in Unity, and only for
// GetToken where there is a real, reproductible issue.
Future<std::string> User::GetTokenThreadSafe(bool force_refresh) {
  Future<std::string> future = GetToken(force_refresh);
  if (future.status() != kFutureStatusPending) {
    return future;
  } else {
    FutureBase base =
        auth_data_->future_impl.LastResultProxy(kUserFn_GetToken);
    const FutureBase& rFuture = base;
    return static_cast<const Future<std::string>&>(rFuture);
  }
}
#endif  // defined(INTERNAL_EXPERIMENTAL)

// Non-inline implementation of UserInfoInterface's virtual destructor
// to prevent its vtable being emitted in each translation unit.
UserInfoInterface::~UserInfoInterface() {}

}  // namespace auth
}  // namespace firebase
