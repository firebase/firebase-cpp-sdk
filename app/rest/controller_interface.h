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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_CONTROLLER_INTERFACE_H_
#define FIREBASE_APP_CLIENT_CPP_REST_CONTROLLER_INTERFACE_H_

#include <cstdint>

namespace firebase {
namespace rest {

// An abstract base class that contains the interface necessary to support a
// Controller.
class Controller {
 public:
  virtual ~Controller();

  // Pauses whatever the handle is doing.
  virtual bool Pause() = 0;

  // Resumes a paused operation.
  virtual bool Resume() = 0;

  // Returns true if the operation is paused.
  virtual bool IsPaused() = 0;

  // Cancels a request and disposes of any resources allocated as part of the
  // request.
  virtual bool Cancel() = 0;

  // Returns a float in the range [0.0 - 1.0], representing the progress of the
  // operation so far, if known. (percent bytes transferred, usually.)
  virtual float Progress() = 0;

  // Returns the number of bytes being transferred in this operation. (either
  // upload or download.)
  virtual int64_t TransferSize() = 0;

  // Returns the number of bytes that have been transferred so far.
  virtual int64_t BytesTransferred() = 0;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_CONTROLLER_INTERFACE_H_
