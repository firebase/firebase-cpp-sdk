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

#ifndef FIREBASE_APP_REST_TRANSFER_INTERFACE_H_
#define FIREBASE_APP_REST_TRANSFER_INTERFACE_H_

namespace firebase {
namespace rest {

class Transfer {
 public:
  virtual ~Transfer() {}

  // Mark the transfer completed.
  virtual void MarkCompleted() = 0;

  // Mark the transfer failed, usually from cancellation or timeout.
  virtual void MarkFailed() = 0;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_TRANSFER_INTERFACE_H_
