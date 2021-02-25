#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LISTENER_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LISTENER_IOS_H_

#include <functional>
#include <memory>
#include <utility>

#include "firestore/src/common/util.h"
#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/promise_ios.h"
#include "firebase/firestore/firestore_errors.h"
#include "Firestore/core/src/core/event_listener.h"
#include "Firestore/core/src/util/statusor.h"

namespace firebase {
namespace firestore {

// Creates an `EventListener` that will:
// - fulfill or fail the given `promise` upon invocation;
// - convert the Core API value given to it upon invocation (`From`) into
//   a public API type (`To`).
template <typename From, typename To>
std::unique_ptr<core::EventListener<From>> ListenerWithPromise(
    Promise<To> promise) {
  return core::EventListener<From>::Create(
      [promise](util::StatusOr<From> maybe_value) mutable {
        if (maybe_value.ok()) {
          From from = std::move(maybe_value).ValueOrDie();
          To to = MakePublic(std::move(from));
          promise.SetValue(std::move(to));

        } else {
          promise.SetError(maybe_value.status());
        }
      });
}

// Creates an `EventListener` that will:
// - invoke the given `callback` with either a valid value and
//   `Error::kErrorOk`, or a default-constructed value and an error indicating
//   the failure;
// - convert the Core API value given to it upon invocation (`From`) into
//   a public API type (`To`).
template <typename From, typename To>
std::unique_ptr<core::EventListener<From>> ListenerWithCallback(
    std::function<void(To, Error, const std::string&)> callback) {
  return core::EventListener<From>::Create(
      [callback](util::StatusOr<From> maybe_value) mutable {
        if (maybe_value.ok()) {
          From from = std::move(maybe_value).ValueOrDie();
          To to = MakePublic(std::move(from));
          callback(std::move(to), Error::kErrorOk, EmptyString());
        } else {
          callback(To{}, maybe_value.status().code(),
                   maybe_value.status().error_message());
        }
      });
}

inline std::unique_ptr<core::EventListener<util::Empty>> ListenerWithCallback(
    std::function<void()> callback) {
  return core::EventListener<util::Empty>::Create(
      [callback](util::StatusOr<util::Empty> maybe_value) mutable {
        callback();
      });
}

// Creates an `EventListener` that will:
// - invoke `OnEvent` on the given `listener`;
// - convert the Core API value given to it upon invocation (`From`) into
//   a public API type (`To`).
// This function assumes that the `listener` given to it is valid for the
// duration of the call.
template <typename From, typename To>
std::unique_ptr<core::EventListener<From>> ListenerWithEventListener(
    EventListener<To>* listener) {
  return ListenerWithCallback<From, To>(
      [listener](To result, Error error_code,
                 const std::string& error_message) {
        listener->OnEvent(result, error_code, error_message);
      });
}

inline util::StatusCallback StatusCallbackWithPromise(Promise<void> promise) {
  return [promise](const util::Status& status) mutable {
    if (status.ok()) {
      promise.SetValue();
    } else {
      promise.SetError(status);
    }
  };
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_LISTENER_IOS_H_
