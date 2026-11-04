// Copyright 2016 Google LLC
#include "storage/src/include/firebase/storage/controller.h"

#include <cstdint>

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/controller_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/controller_ios.h"
#else
#include "storage/src/desktop/controller_desktop.h"
#endif

namespace firebase::storage {

Controller::Controller() : internal_(new internal::ControllerInternal()) {}
Controller::Controller(internal::ControllerInternal* internal)
    : internal_(internal) {}
Controller::Controller(const Controller& other)
    : internal_(other.internal_
                    ? new internal::ControllerInternal(*other.internal_)
                    : nullptr) {}

Controller& Controller::operator=(const Controller& other) {
  if (this != &other) {
    auto* next = other.internal_
                     ? new internal::ControllerInternal(*other.internal_)
                     : nullptr;
    delete internal_;
    internal_ = next;
  }
  return *this;
}

Controller::Controller(Controller&& other) noexcept
    : internal_(other.internal_) {
  other.internal_ = nullptr;
}

Controller& Controller::operator=(Controller&& other) noexcept {
  if (this != &other) {
    delete internal_;
    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}

Controller::~Controller() { delete internal_; }

bool Controller::Pause() { return internal_ && internal_->Pause(); }
bool Controller::Resume() { return internal_ && internal_->Resume(); }
bool Controller::Cancel() { return internal_ && internal_->Cancel(); }
bool Controller::is_paused() const {
  return internal_ && internal_->is_paused();
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
bool Controller::is_valid() const { return internal_ && internal_->is_valid(); }

bool Controller::operator==(const Controller& other) const {
  if (internal_ == other.internal_) return true;
  if (is_valid() != other.is_valid()) return false;
  return !is_valid() || *internal_ == *other.internal_;
}

}  // namespace firebase::storage
