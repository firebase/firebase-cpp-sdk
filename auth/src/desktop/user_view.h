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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_USER_VIEW_H_
#define FIREBASE_AUTH_SRC_DESKTOP_USER_VIEW_H_

#include <assert.h>

#include <memory>
#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/provider_user_info.h"
#include "auth/src/desktop/user_desktop.h"

namespace firebase {
namespace auth {

class UserView {
 public:
  class Reader {
   public:
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    Reader(Reader&& rhs) noexcept
        : user_data_(rhs.user_data_),
          auth_data_(rhs.auth_data_),
          mutex_(rhs.mutex_) {
      rhs.mutex_ = nullptr;
    }

    Reader& operator=(Reader&& rhs) noexcept {
      if (this != &rhs) {
        if (mutex_) mutex_->Release();
        user_data_ = rhs.user_data_;
        auth_data_ = rhs.auth_data_;
        mutex_ = rhs.mutex_;
        rhs.mutex_ = nullptr;
      }
      return *this;
    }

    bool IsValid() const { return user_data_ != nullptr; }
    const UserData* operator->() const { return user_data_; }
    const UserData& operator*() const { return *user_data_; }
    const std::vector<UserInfoInterface*>& GetUserInfos() const;

    ~Reader() {
      if (mutex_) mutex_->Release();
    }

   private:
    friend class UserView;

    Reader() : user_data_(nullptr), auth_data_(nullptr), mutex_(nullptr) {}
    Reader(Mutex* const mutex, const UserData* const user_data,
           const AuthData& auth_data)
        : user_data_(user_data), auth_data_(&auth_data), mutex_(mutex) {
      FIREBASE_ASSERT(mutex);
    }

    const UserData* user_data_;
    const AuthData* auth_data_;
    Mutex* mutex_;
  };

  class Writer {
   public:
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    Writer(Writer&& rhs) noexcept
        : user_data_(rhs.user_data_),
          auth_data_(rhs.auth_data_),
          mutex_(rhs.mutex_) {
      rhs.mutex_ = nullptr;
    }

    Writer& operator=(Writer&& rhs) noexcept {
      if (this != &rhs) {
        if (mutex_) mutex_->Release();
        user_data_ = rhs.user_data_;
        auth_data_ = rhs.auth_data_;
        mutex_ = rhs.mutex_;
        rhs.mutex_ = nullptr;
      }
      return *this;
    }

    bool IsValid() const { return user_data_ != nullptr; }
    UserData* operator->() { return user_data_; }
    UserData& operator*() { return *user_data_; }

    void ResetUserInfos(const std::vector<UserInfoImpl>& provider_data);

    ~Writer() {
      if (mutex_) mutex_->Release();
    }

   private:
    friend class UserView;

    Writer() : user_data_(nullptr), auth_data_(nullptr), mutex_(nullptr) {}
    Writer(Mutex* const mutex, UserData* const user_data, AuthData& auth_data)
        : user_data_(user_data), auth_data_(&auth_data), mutex_(mutex) {
      FIREBASE_ASSERT(mutex);
    }

    UserData* user_data_;
    AuthData* auth_data_;
    Mutex* mutex_;
  };

  explicit UserView(const UserData& user_data) : user_data_(user_data) {}

  const UserData& user_data() const { return user_data_; }

  static Writer ResetUser(AuthData* auth_data, const UserData& user_data,
                          UserData* const out_previous_user = nullptr);
  static void ClearUser(AuthData* auth_data,
                        UserData* out_previous_user = nullptr);
  static Reader GetReader(AuthData* auth_data);

  template <typename CallbackT>
  static bool TryRead(AuthData* const auth_data, CallbackT callback) {
    const Reader reader = GetReader(auth_data);
    if (!reader.IsValid()) return false;
    callback(reader);
    return true;
  }

  static Writer GetWriter(AuthData* auth_data);

 private:
  static UserView* CastToUser(AuthData* auth_data);
  UserData user_data_;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_USER_VIEW_H_
