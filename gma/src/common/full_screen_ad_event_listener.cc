/*
 * Copyright 2021 Google LLC
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

#include "gma/src/common/full_screen_ad_event_listener.h"

#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {
namespace gma {
namespace internal {

void FullScreenAdEventListener::SetFullScreenContentListener(
    FullScreenContentListener* listener) {
  MutexLock lock(listener_mutex_);
  full_screen_content_listener_ = listener;
}

void FullScreenAdEventListener::SetPaidEventListener(
    PaidEventListener* listener) {
  MutexLock lock(listener_mutex_);
  paid_event_listener_ = listener;
}

void FullScreenAdEventListener::NotifyListenerOfAdClickedFullScreenContent() {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdClicked();
  }
}

void FullScreenAdEventListener::NotifyListenerOfAdDismissedFullScreenContent() {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdDismissedFullScreenContent();
  }
}

void FullScreenAdEventListener::NotifyListenerOfAdFailedToShowFullScreenContent(
    const AdError& ad_error) {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdFailedToShowFullScreenContent(ad_error);
  }
}

void FullScreenAdEventListener::NotifyListenerOfAdImpression() {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdImpression();
  }
}

void FullScreenAdEventListener::NotifyListenerOfAdShowedFullScreenContent() {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdShowedFullScreenContent();
  }
}

void FullScreenAdEventListener::NotifyListenerOfPaidEvent(
    const AdValue& ad_value) {
  MutexLock lock(listener_mutex_);
  if (paid_event_listener_ != nullptr) {
    paid_event_listener_->OnPaidEvent(ad_value);
  }
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
