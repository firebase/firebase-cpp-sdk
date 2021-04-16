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

#include <condition_variable>  // NOLINT

#include "remote_config/src/desktop/notification_channel.h"

namespace firebase {
namespace remote_config {
namespace internal {

NotificationChannel::NotificationChannel()
    : have_item_(false), closed_(false) {}

bool NotificationChannel::Get() {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_variable_.wait(lock, [this]() { return closed_ || have_item_; });
  have_item_ = false;
  return !closed_;
}

void NotificationChannel::Put() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!closed_) {
    have_item_ = true;
    condition_variable_.notify_one();
  }
}

void NotificationChannel::Close() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!closed_) {
    closed_ = true;
    condition_variable_.notify_all();
  }
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
