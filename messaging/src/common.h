// Copyright 2016 Google LLC
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
#ifndef FIREBASE_MESSAGING_CLIENT_CPP_SRC_COMMON_H_
#define FIREBASE_MESSAGING_CLIENT_CPP_SRC_COMMON_H_

#include "app/src/reference_counted_future_impl.h"
#include "messaging/src/include/firebase/messaging.h"

namespace firebase {
namespace messaging {

namespace internal {

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy();

// Unregisters the cleanup task for this module if auto-initialization is
// disabled.
void UnregisterTerminateOnDefaultAppDestroy();

// Returns whether the module is initialized, implemented for each platform.
bool IsInitialized();

}  // namespace internal

enum MessagingFn {
  kMessagingFnRequestPermission,
  kMessagingFnSubscribe,
  kMessagingFnUnsubscribe,
  kMessagingFnGetToken,
  kMessagingFnDeleteToken,
  kMessagingFnCount
};

// Data structure which holds the Future API implementation with the only
// future required by this API (fetch_future_).
class FutureData {
 public:
  FutureData() : api_(kMessagingFnCount) {}
  ~FutureData() {}

  ReferenceCountedFutureImpl* api() { return &api_; }

  // Create FutureData singleton.
  static FutureData* Create();
  // Destroy the FutureData singleton.
  static void Destroy();
  // Get the Future data singleton.
  static FutureData* Get();

 private:
  ReferenceCountedFutureImpl api_;

  static FutureData* s_future_data_;
};

// Implemented for each platform to notify the implementation of a new
// listener.
void NotifyListenerSet(Listener* listener);

// Determine whether a listener is present.
bool HasListener();

// Override the current listener if the supplied listener is not null.
Listener* SetListenerIfNotNull(Listener* listener);

// Notify the currently set listener of a new message.
void NotifyListenerOnMessage(const Message& message);

// Notify the currently set listener of a new token.
void NotifyListenerOnTokenReceived(const char* token);

}  // namespace messaging
}  // namespace firebase

#endif  // FIREBASE_MESSAGING_CLIENT_CPP_SRC_COMMON_H_
