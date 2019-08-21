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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_USER_VIEW_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_USER_VIEW_H_

#include <memory>
#include <utility>
#include "app/src/assert.h"
#include "app/src/mutex.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/provider_user_info.h"
#include "auth/src/desktop/user_desktop.h"

namespace firebase {
namespace auth {

// Intended to make accessing and modifying the currently signed-in user
// thread-safe. All operations are protected by AuthData::future_impl.mutex().
class UserView {
 public:
  // Thread-safe read-only view of the currently signed-in user.
  //
  // If there exists a currently signed-in user, IsValid() will return true, and
  // user's data can be accessed using pointer-like semantics. User is protected
  // by a mutex lock; Reader uses RAII to hold the lock as long as this Reader
  // exists, so be careful *not* to try to acquire lock on auth_data's
  // future_impl mutex while holding a reference to Reader!
  //
  // If there is no currently signed-in user, IsValid() will return false. In
  // this case, don't try to access the underlying data, which will be null. No
  // mutex lock is associated with an invalid Reader.
  class Reader {
   public:
    bool IsValid() const { return user_data_ != nullptr; }
    const UserData* operator->() const { return user_data_; }
    const std::vector<UserInfoInterface*>& GetUserInfos() const;

    ~Reader() {
      if (mutex_) {
        mutex_->Release();
      }
    }

   private:
    friend class UserView;

    Reader() : user_data_(nullptr), auth_data_(nullptr), mutex_(nullptr) {}
    Reader(Mutex* const mutex, const UserData* const user_data,
           const AuthData& auth_data)
        : user_data_(user_data), auth_data_(&auth_data), mutex_(mutex) {
      FIREBASE_ASSERT(mutex);
    }

    const UserData* const user_data_;
    const AuthData* const auth_data_;
    Mutex* mutex_;
  };

  // Thread-safe read-write view of the currently signed-in user.
  //
  // Similar to UserView::Reader, but also allows modifying the signed-in user's
  // attributes using pointer-like access. Additionally, provides operations to
  // reset or clear UserInfos associated with the current user.
  class Writer {
   public:
    bool IsValid() const { return user_data_ != nullptr; }
    UserData* operator->() { return user_data_; }

    void ResetUserInfos(const std::vector<UserInfoImpl>& provider_data);
    void ClearUserInfos();

    ~Writer() {
      if (mutex_) {
        mutex_->Release();
      }
    }

   private:
    friend class UserView;

    Writer() : user_data_(nullptr), auth_data_(nullptr), mutex_(nullptr) {}
    Writer(Mutex* const mutex, UserData* const user_data, AuthData& auth_data)
        : user_data_(user_data), auth_data_(&auth_data), mutex_(mutex) {
      FIREBASE_ASSERT(mutex);
    }

    UserData* const user_data_;
    AuthData* const auth_data_;
    Mutex* mutex_;
  };

  // Construct a user view from an existing set of user data.
  explicit UserView(const UserData& user_data) : user_data_(user_data) {}

  // Exposed for testing.
  const UserData& user_data() const { return user_data_; }

  // Resets the currently signed-in user with the given user_data and returns
  // a writeable view of the user for additional reads or modifications (e.g.,
  // to update UserInfos). Optionally, provide out_previous_user to save the
  // previous user's state before it's overwritten.
  // Thread-safe.
  static Writer ResetUser(AuthData* auth_data, const UserData& user_data,
                          UserData* const out_previous_user = nullptr);

  // Deletes the currently signed-in user and clears UserInfos. Optionally,
  // provide out_previous_user to save the previous user's state before it's
  // overwritten.
  //
  // If there is no currently signed-in user, this is a no-op.
  //
  // Thread-safe.
  static void ClearUser(AuthData* auth_data,
                        UserData* out_previous_user = nullptr);

  // Returns a read-only view of the currently signed-in user, if any. Be
  // careful to check Reader.IsValid() before accessing!
  // Thread-safe.
  static Reader GetReader(AuthData* auth_data);

  // If there is a currently signed-in user, invokes the given callback and
  // returns true. Otherwise, doesn't invoke the callback and returns false.
  // This is intended to minimize the chances of forgetting to check for
  // IsValid.
  //
  // Callback can be a stateful lambda that will read the attributes you're
  // interested in, for example:
  //
  // @code{.cpp}
  // std::string uid, token;
  // const bool is_user_signed_in = [&](const UserView::Reader& user) {
  //   uid = user->uid;
  //   token = user->id_token;
  // });
  // if (!is_user_signed_in) {
  //   // Handle failure; uid and token haven't been touched
  // } else {
  //   // Handle success; uid and token have been read.
  // }
  // @endcode
  //
  // Thread-safe.
  template <typename CallbackT>
  static bool TryRead(AuthData* auth_data, CallbackT callback);

  // Returns a read-write view of the currently signed-in user, if any. Be
  // careful to check Writer.IsValid() before accessing!
  // Thread-safe.
  static Writer GetWriter(AuthData* auth_data);

 private:
  static UserView* CastToUser(AuthData* auth_data);

  UserData user_data_;
};

// Implementation

template <typename CallbackT>
inline bool UserView::TryRead(AuthData* const auth_data, CallbackT callback) {
  const Reader reader = GetReader(auth_data);
  if (!reader.IsValid()) {
    return false;
  }
  callback(reader);
  return true;
}

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_USER_VIEW_H_
