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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_NOTIFICATION_CHANNEL_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_NOTIFICATION_CHANNEL_H_

#include <condition_variable>  // NOLINT

namespace firebase {
namespace remote_config {
namespace internal {

// Provides a wait queue with a blocking `Get` function.
// 'Put' is used to unblock a single 'Get' call, and 'Close' unblocks all
// current 'Get' calls and prevents future blocking for the instance.
class NotificationChannel {
 public:
  NotificationChannel();

  // Blocks until Put() or Close() is called on another thread.
  // Returns false immediately, if Close() is or has already been called,
  // otherwise true if unblocked by a call to `Put`.
  // If `Get` is called after `Put` but nothing was unblocked at the time, the
  // thread will still block until `Put` is called again.
  bool Get();

  // Unblocks one thread waiting for a result from `Get`.
  // If 'Close' has already been called, 'Put' does nothing.
  void Put();

  // Closes the queue. All threads waiting on 'Get' will be unblocked and
  // returned false.
  void Close();

 private:
  bool have_item_;
  bool closed_;

  std::mutex mutex_;
  std::condition_variable condition_variable_;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_NOTIFICATION_CHANNEL_H_
