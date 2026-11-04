// Copyright 2016 Google LLC
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

#ifndef FIREBASE_AUTH_SRC_COMMON_H_
#define FIREBASE_AUTH_SRC_COMMON_H_

#include <string>

#include "auth/src/common/promise.h"
#include "auth/src/data.h"

namespace firebase {
namespace auth {

enum CredentialApiFunction {
  kCredentialFn_GameCenterGetCredential,
  kNumCredentialFunctions
};

void* CreatePlatformAuth(App* app);
void InitPlatformAuth(AuthData* auth_data);
void DestroyPlatformAuth(AuthData* auth_data);
void LogHeartbeat(Auth* auth);

#define AUTH_RESULT_FN(class_name, fn_name, result_type)                  \
  Future<result_type> class_name::fn_name##LastResult() const {           \
    if (!auth_data_) return Future<result_type>();                        \
    return static_cast<const Future<result_type>&>(                       \
        auth_data_->future_impl.LastResult(k##class_name##Fn_##fn_name)); \
  }

inline bool ValidUser(const AuthData* auth_data) {
  if (!auth_data) return false;
  MutexLock lock(*const_cast<Mutex*>(&auth_data->future_impl.mutex()));
  return auth_data->user_impl != nullptr;
}

void NotifyAuthStateListeners(AuthData* auth_data);
void NotifyIdTokenListeners(AuthData* auth_data);
void UpdateCurrentUser(AuthData* auth_data);
ReferenceCountedFutureImpl* GetCredentialFutureImpl();
void CleanupCredentialFutureImpl();

void SignInWithCustomToken(AuthData* auth_data, Promise<AuthResult> promise,
                           const char* custom_token);
void SignInWithCredential(AuthData* auth_data, Promise<User> promise,
                          const Credential& credential);
Future<AuthResult> SignInWithProvider(AuthData* auth_data,
                                      FederatedAuthProvider* provider);
void SignInAnonymously(AuthData* auth_data, Promise<AuthResult> promise);
void SignInWithEmailAndPassword(AuthData* auth_data,
                                Promise<AuthResult> promise, const char* email,
                                const char* password);
void CreateUserWithEmailAndPassword(AuthData* auth_data,
                                    Promise<AuthResult> promise,
                                    const char* email, const char* password);
void SignInAndRetrieveDataWithCredential(AuthData* auth_data,
                                         Promise<AuthResult> promise,
                                         const Credential& credential);
void FetchProvidersForEmail(AuthData* auth_data,
                            Promise<Auth::FetchProvidersResult> promise,
                            const char* email);
void SendPasswordResetEmail(AuthData* auth_data, Promise<void> promise,
                            const char* email);
void GetToken(AuthData* auth_data, Promise<std::string> promise,
              bool force_refresh);
void GetTokenInternal(AuthData* auth_data, Promise<std::string> promise,
                      bool force_refresh, int future_identifier);
void GetIdTokenResult(AuthData* auth_data, Promise<IdTokenResult> promise,
                      bool force_refresh);
void Delete(AuthData* auth_data, Promise<void> promise);
void SendEmailVerification(AuthData* auth_data, Promise<void> promise);
void SendEmailVerificationBeforeUpdatingEmail(AuthData* auth_data,
                                              Promise<void> promise,
                                              const char* email);
void Reload(AuthData* auth_data, Promise<void> promise);
void UpdatePassword(AuthData* auth_data, Promise<void> promise,
                    const char* password);
void UpdateUserProfile(AuthData* auth_data, Promise<void> promise,
                       const User::UserProfile& profile);
void Unlink(AuthData* auth_data, Promise<AuthResult> promise,
            const char* provider);
void LinkWithCredential(AuthData* auth_data, Promise<AuthResult> promise,
                        const Credential& credential);
Future<AuthResult> LinkWithProvider(AuthData* auth_data,
                                    FederatedAuthProvider* provider);
void Reauthenticate(AuthData* auth_data, Promise<void> promise,
                    const Credential& credential);
void ReauthenticateAndRetrieveData(AuthData* auth_data,
                                   Promise<AuthResult> promise,
                                   const Credential& credential);
Future<AuthResult> ReauthenticateWithProvider(AuthData* auth_data,
                                              FederatedAuthProvider* provider);
void UpdatePhoneNumberCredential(AuthData* auth_data, Promise<User> promise,
                                 const PhoneAuthCredential& credential);
void SignOut(AuthData* auth_data);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_COMMON_H_
