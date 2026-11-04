// Copyright 2017 Google LLC
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

#include "auth/src/desktop/user_view.h"

namespace firebase {
namespace auth {

namespace {
void DoResetUserInfos(AuthData* const auth_data,
                      const std::vector<UserInfoImpl>& provider_data) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  ClearUserInfos(auth_data);
  for (const auto& user_info : provider_data) {
    auth_data->user_infos.push_back(new UserInfoInterfaceImpl(user_info));
  }
}
}  // namespace

const std::vector<UserInfoInterface*>& UserView::Reader::GetUserInfos() const {
  static const std::vector<UserInfoInterface*> kEmptyUserInfos;
  return auth_data_ ? auth_data_->user_infos : kEmptyUserInfos;
}

void UserView::Writer::ResetUserInfos(
    const std::vector<UserInfoImpl>& provider_data) {
  DoResetUserInfos(auth_data_, provider_data);
}

UserView::Writer UserView::ResetUser(AuthData* const auth_data,
                                     const UserData& user_data,
                                     UserData* const out_previous_user) {
  FIREBASE_ASSERT_RETURN(Writer(), auth_data);
  Mutex& mutex = auth_data->future_impl.mutex();
  mutex.Acquire();
  if (UserView* user = CastToUser(auth_data)) {
    if (out_previous_user) *out_previous_user = std::move(user->user_data_);
    delete user;
  }
  auth_data->user_impl = new UserView(user_data);
  return Writer(&mutex, &CastToUser(auth_data)->user_data_, *auth_data);
}

void UserView::ClearUser(AuthData* const auth_data,
                         UserData* const out_previous_user) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  MutexLock lock(auth_data->future_impl.mutex());
  if (UserView* user = CastToUser(auth_data)) {
    if (out_previous_user) *out_previous_user = std::move(user->user_data_);
    delete user;
  }
  auth_data->user_impl = nullptr;
  DoResetUserInfos(auth_data, {});
}

UserView::Reader UserView::GetReader(AuthData* const auth_data) {
  FIREBASE_ASSERT_RETURN(Reader(), auth_data);
  Mutex& mutex = auth_data->future_impl.mutex();
  mutex.Acquire();
  if (const UserView* const user = CastToUser(auth_data)) {
    return Reader(&mutex, &user->user_data_, *auth_data);
  }
  mutex.Release();
  return Reader();
}

UserView::Writer UserView::GetWriter(AuthData* const auth_data) {
  FIREBASE_ASSERT_RETURN(Writer(), auth_data);
  Mutex& mutex = auth_data->future_impl.mutex();
  mutex.Acquire();
  if (UserView* const user = CastToUser(auth_data)) {
    return Writer(&mutex, &user->user_data_, *auth_data);
  }
  mutex.Release();
  return Writer();
}

UserView* UserView::CastToUser(AuthData* const auth_data) {
  FIREBASE_ASSERT_RETURN(nullptr, auth_data);
  return static_cast<UserView*>(auth_data->user_impl);
}

}  // namespace auth
}  // namespace firebase
