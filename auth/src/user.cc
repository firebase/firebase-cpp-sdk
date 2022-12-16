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

#if defined(__ANDROID__)
#include "auth/src/android/user_android.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace auth {

#if defined(__ANDROID__)
#define AUTH_USER_RESULT_FN(class_name, fn_name, result_type)              \
  Future<result_type> class_name::fn_name##LastResult() const {            \
    return static_cast<const Future<result_type>&>(                        \
        internal_->future_api()->LastResult(k##class_name##Fn_##fn_name)); \
  }
#else
// All the result functions are similar.
// Just return the local Future, cast to the proper result type.
#define AUTH_USER_RESULT_FN(class_name, fn_name, result_type)             \
  Future<result_type> class_name::fn_name##LastResult() const {           \
    return static_cast<const Future<result_type>&>(                       \
        auth_data_->future_impl.LastResult(k##class_name##Fn_##fn_name)); \
  }
#endif  // defined(__ANDROID__)

AUTH_USER_RESULT_FN(User, GetToken, std::string)
AUTH_USER_RESULT_FN(User, UpdateEmail, void)
AUTH_USER_RESULT_FN(User, UpdatePassword, void)
AUTH_USER_RESULT_FN(User, Reauthenticate, void)
AUTH_USER_RESULT_FN(User, ReauthenticateAndRetrieveData, SignInResult)
AUTH_USER_RESULT_FN(User, SendEmailVerification, void)
AUTH_USER_RESULT_FN(User, UpdateUserProfile, void)
AUTH_USER_RESULT_FN(User, LinkWithCredential, User*)
AUTH_USER_RESULT_FN(User, LinkAndRetrieveDataWithCredential, SignInResult)
AUTH_USER_RESULT_FN(User, Unlink, User*)
AUTH_USER_RESULT_FN(User, UpdatePhoneNumberCredential, User*)
AUTH_USER_RESULT_FN(User, Reload, void)
AUTH_USER_RESULT_FN(User, Delete, void)

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
#if defined(__ANDROID__)
    FutureBase base =
        internal_->future_api()->LastResultProxy(kUserFn_GetToken);
#else
    FutureBase base = auth_data_->future_impl.LastResultProxy(kUserFn_GetToken);
#endif  // defined(__ANDROID__)
    const FutureBase& rFuture = base;
    return static_cast<const Future<std::string>&>(rFuture);
  }
}
#endif  // defined(INTERNAL_EXPERIMENTAL)

bool User::is_valid() const {
#if defined(__ANDROID__)
  return internal_ && internal_->is_valid();
#else
  return ValidUser(auth_data_);
#endif  // defined(__ANDROID__)
}

// Non-inline implementation of UserInfoInterface's virtual destructor
// to prevent its vtable being emitted in each translation unit.
UserInfoInterface::~UserInfoInterface() {}

}  // namespace auth
}  // namespace firebase
