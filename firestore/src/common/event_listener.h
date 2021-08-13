/*
 * Copyright 2018 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_COMMON_EVENT_LISTENER_H_
#define FIREBASE_FIRESTORE_SRC_COMMON_EVENT_LISTENER_H_

#include <string>

#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {

/**
 * An interface for event listeners.
 *
 * TODO(b/191976416): This class was originally written to facilitate using the
 * STLPort C++ runtime library. STLPort is no longer supported. We can consider
 * removing this class.
 */
template <typename T>
class EventListener {
 public:
  virtual ~EventListener() = default;

  /**
   * @brief OnEvent will be called with the new value or the error if an error
   * occurred.
   *
   * It's guaranteed that value is valid if and only if error is
   * Error::kErrorOk.
   *
   * @param value The value of the event. Invalid if there was an error.
   * @param error_code The error code if there was an error. Error::kErrorOk
   * otherwise.
   * @param error_message The error message if there was an error. An empty
   * string otherwise.
   */
  virtual void OnEvent(const T& value,
                       Error error_code,
                       const std::string& error_message) = 0;
};

/**
 * Interface used for void EventListeners that don't produce a value.
 */
template <>
class EventListener<void> {
 public:
  virtual ~EventListener() = default;

  /**
   * @brief OnEvent will be called with the error if an error occurred.
   *
   * @param error_code The error code if there was an error. Error::kErrorOk
   * otherwise.
   * @param error_message The error message if there was an error. An empty
   * string otherwise.
   */
  virtual void OnEvent(Error error_code, const std::string& error_message) = 0;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_EVENT_LISTENER_H_
