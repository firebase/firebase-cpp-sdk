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

#include "app/src/include/firebase/internal/platform.h"

// Controller is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/controller_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "storage/src/ios/controller_ios.h"
#else
#include "storage/src/desktop/controller_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

namespace firebase {
namespace storage {

Controller::Controller() : internal_(new internal::ControllerInternal()) {}

Controller::Controller(internal::ControllerInternal* internal)
    : internal_(internal) {}

Controller::Controller(const Controller& other)
    : internal_(other.internal_
                    ? new internal::ControllerInternal(*other.internal_)
                    : nullptr) {}

Controller& Controller::operator=(const Controller& other) {
  if (internal_) delete internal_;
  internal_ = other.internal_
                  ? new internal::ControllerInternal(*other.internal_)
                  : nullptr;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
Controller::Controller(Controller&& other) {
  internal_ = other.internal_;
  other.internal_ = nullptr;
}

Controller& Controller::operator=(Controller&& other) {
  if (internal_) delete internal_;
  internal_ = other.internal_;
  other.internal_ = nullptr;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

Controller::~Controller() {
  if (internal_) delete internal_;
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
  return internal_ ? internal_->total_byte_count() : 0;
}

StorageReference Controller::GetReference() const {
  return internal_ ? StorageReference(internal_->GetReference())
                   : StorageReference(nullptr);
}

bool Controller::is_valid() const {
  return internal_ != nullptr && internal_->is_valid();
}

}  // namespace storage
}  // namespace firebase
