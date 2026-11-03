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

#include "storage/src/include/firebase/storage/controller.h"

#include <cstdint>

#include "app/src/include/firebase/internal/platform.h"

// Controller is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/controller_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/controller_ios.h"
#else
#include "storage/src/desktop/controller_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase::storage {

Controller::Controller() : internal_(new internal::ControllerInternal()) {}

Controller::Controller(internal::ControllerInternal* internal)
    : internal_(internal) {}

Controller::Controller(const Controller& other)
    : internal_(other.internal_
                    ? new internal::ControllerInternal(*other.internal_)
                    : nullptr) {}

Controller& Controller::operator=(const Controller& other) {
  if (this == &other) {
    return *this;
  }
  internal::ControllerInternal* new_internal =
      other.internal_ ? new internal::ControllerInternal(*other.internal_)
                      : nullptr;
  delete internal_;
  internal_ = new_internal;
  return *this;
}

Controller::Controller(Controller&& other) noexcept
    : internal_(other.internal_) {
  other.internal_ = nullptr;
}

Controller& Controller::operator=(Controller&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  delete internal_;
  internal_ = other.internal_;
  other.internal_ = nullptr;
  return *this;
}

Controller::~Controller() {
  delete internal_;
  internal_ = nullptr;
}

bool Controller::Pause() { return internal_ ? internal_->Pause() : false; }

bool Controller::Resume() { return internal_ ? internal_->Resume() : false; }

bool Controller::Cancel() { return internal_ ? internal_->Cancel() : false; }

bool Controller::is_paused() const {
  return internal_ ? internal_->is_paused() : false;
}

int64_t Controller::bytes_transferred() const {
  return internal_ ? internal_->bytes_transferred() : 0;
}

int64_t Controller::total_byte_count() const {
  return internal_ ? internal_->total_byte_count() : -1;
}

StorageReference Controller::GetReference() const {
  return internal_ ? StorageReference(internal_->GetReference())
                   : StorageReference(nullptr);
}

bool Controller::is_valid() const {
  return internal_ != nullptr && internal_->is_valid();
}

bool Controller::operator==(const Controller& other) const {
  if (internal_ == other.internal_) {
    return true;
  }
  const bool this_valid = is_valid();
  const bool other_valid = other.is_valid();
  if (!this_valid && !other_valid) {
    return true;
  }
  if (this_valid != other_valid) {
    return false;
  }
  return *internal_ == *other.internal_;
}

}  // namespace firebase::storage
