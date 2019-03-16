/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_IID_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_IID_H_

#ifdef __ANDROID__
#include <jni.h>
#endif  // __ANDROID__
#include <string>

#include "app/src/include/firebase/app.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace internal {

// Firebase Instance ID provides a unique identifier for each app instance and
// a mechanism to authenticate and authorize actions.
class InstanceId {
 public:
  explicit InstanceId(const App& app);

  ~InstanceId();

  // Returns the master token for an App.
  //
  // NOTE: This can return an empty string if the token is not available yet.
  std::string GetMasterToken() const;

  // TODO(b/65043712): implement an async method that is guaranteed to return a
  // valid token.
  // Future<std::string> GetMasterTokenAsync() const;

 private:
  const App& app_;
#ifdef __ANDROID__
  jobject iid_;
#endif  // __ANDROID__
};

}  // namespace internal
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
#endif  // FIREBASE_APP_CLIENT_CPP_SRC_IID_H_
