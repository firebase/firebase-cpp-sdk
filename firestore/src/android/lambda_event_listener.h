/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_LAMBDA_EVENT_LISTENER_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_LAMBDA_EVENT_LISTENER_H_

#include <functional>

#include "app/meta/move.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/event_listener.h"

namespace firebase {
namespace firestore {

/**
 * A particular EventListener type that wraps a listener provided by user in the
 * form of lambda.
 */
template <typename T>
class LambdaEventListener : public EventListener<T> {
 public:
  LambdaEventListener(
      std::function<void(const T&, Error, const std::string&)> callback)
      : callback_(firebase::Move(callback)) {
    FIREBASE_ASSERT(callback_);
  }

  void OnEvent(const T& value,
               Error error_code,
               const std::string& error_message) override {
    callback_(value, error_code, error_message);
  }

 private:
  std::function<void(const T&, Error, const std::string&)> callback_;
};

template <>
class LambdaEventListener<void> : public EventListener<void> {
 public:
  LambdaEventListener(std::function<void()> callback)
      : callback_(firebase::Move(callback)) {
    FIREBASE_ASSERT(callback_);
  }

  void OnEvent(Error, const std::string&) override { callback_(); }

 private:
  std::function<void()> callback_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_LAMBDA_EVENT_LISTENER_H_
