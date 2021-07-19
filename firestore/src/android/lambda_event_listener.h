// Copyright 2020 Google LLC

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
