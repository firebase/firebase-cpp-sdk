// Copyright 2021 Google LLC
#ifndef FIREBASE_FIRESTORE_SRC_MAIN_LISTENER_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_LISTENER_MAIN_H_

#include <functional>
#include <memory>
#include <utility>
#include "Firestore/core/src/core/event_listener.h"
#include "Firestore/core/src/util/statusor.h"
#include "app/src/log.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/util.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/promise_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase::firestore {

template <typename From, typename To>
std::unique_ptr<core::EventListener<From>> ListenerWithPromise(Promise<To> promise) {
  return core::EventListener<From>::Create(
      [promise = std::move(promise)](util::StatusOr<From> maybe_value) mutable {
        if (maybe_value.ok()) {
          promise.SetValue(MakePublic(std::move(maybe_value).ValueOrDie()));
        } else {
          promise.SetError(maybe_value.status());
        }
      });
}

template <typename From, typename To>
std::unique_ptr<core::EventListener<From>> ListenerWithCallback(
    std::function<void(To, Error, const std::string&)> callback) {
  return core::EventListener<From>::Create(
      [callback = std::move(callback)](util::StatusOr<From> maybe_value) {
        if (!callback) return;
        if (maybe_value.ok()) {
          callback(MakePublic(std::move(maybe_value).ValueOrDie()), Error::kErrorOk, EmptyString());
        } else {
          callback(To{}, maybe_value.status().code(), maybe_value.status().error_message());
        }
      });
}

inline std::unique_ptr<core::EventListener<util::Empty>> ListenerWithCallback(
    std::function<void()> callback) {
  return core::EventListener<util::Empty>::Create(
      [callback = std::move(callback)](util::StatusOr<util::Empty> maybe_value) {
        if (!maybe_value.ok()) {
          LogError("Listener failed with error: %s", maybe_value.status().error_message().c_str());
          return;
        }
        if (callback) callback();
      });
}

template <typename From, typename To>
std::unique_ptr<core::EventListener<From>> ListenerWithEventListener(EventListener<To>* listener) {
  return ListenerWithCallback<From, To>(
      [listener](To result, Error error_code, const std::string& error_message) {
        listener->OnEvent(result, error_code, error_message);
      });
}

inline util::StatusCallback StatusCallbackWithPromise(Promise<void> promise) {
  return [promise = std::move(promise)](const util::Status& status) mutable {
    if (status.ok()) {
      promise.SetValue();
    } else {
      promise.SetError(status);
    }
  };
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_LISTENER_MAIN_H_
