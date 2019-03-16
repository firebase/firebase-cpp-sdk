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

#include "database/src/include/firebase/database/disconnection.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

// DisconnectionHandlerInternal is defined in these 3 files, one implementation
// for each OS.
#if defined(__ANDROID__)
#include "database/src/android/database_android.h"
#include "database/src/android/disconnection_android.h"
#elif TARGET_OS_IPHONE
#include "database/src/ios/database_ios.h"
#include "database/src/ios/disconnection_ios.h"
#elif defined(FIREBASE_TARGET_DESKTOP)
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/disconnection_desktop.h"
#else
#include "database/src/stub/database_stub.h"
#include "database/src/stub/disconnection_stub.h"
#endif  // defined(__ANDROID__), TARGET_OS_IPHONE

#include "database/src/common/cleanup.h"

namespace firebase {
namespace database {

using internal::DisconnectionHandlerInternal;

typedef CleanupFn<DisconnectionHandler, DisconnectionHandlerInternal>
    CleanupFnDisconnectionHandler;

template <>
CleanupFnDisconnectionHandler::CreateInvalidObjectFn
    CleanupFnDisconnectionHandler::create_invalid_object =
        DisconnectionHandlerInternal::GetInvalidDisconnectionHandler;

DisconnectionHandler::DisconnectionHandler(
    DisconnectionHandlerInternal* internal)
    : internal_(internal) {
  CleanupFnDisconnectionHandler::Register(this, internal_);
}

DisconnectionHandler::~DisconnectionHandler() {
  CleanupFnDisconnectionHandler::Unregister(this, internal_);
  if (internal_) {
    delete internal_;
  }
}

Future<void> DisconnectionHandler::Cancel() {
  return internal_ ? internal_->Cancel() : Future<void>();
}

Future<void> DisconnectionHandler::CancelLastResult() {
  return internal_ ? internal_->CancelLastResult() : Future<void>();
}

Future<void> DisconnectionHandler::RemoveValue() {
  return internal_ ? internal_->RemoveValue() : Future<void>();
}

Future<void> DisconnectionHandler::RemoveValueLastResult() {
  return internal_ ? internal_->RemoveValueLastResult() : Future<void>();
}

Future<void> DisconnectionHandler::SetValue(Variant value) {
  return internal_ ? internal_->SetValue(value) : Future<void>();
}

Future<void> DisconnectionHandler::SetValueLastResult() {
  return internal_ ? internal_->SetValueLastResult() : Future<void>();
}

Future<void> DisconnectionHandler::SetValueAndPriority(Variant value,
                                                       Variant priority) {
  return internal_ ? internal_->SetValueAndPriority(value, priority)
                   : Future<void>();
}

Future<void> DisconnectionHandler::SetValueAndPriorityLastResult() {
  return internal_ ? internal_->SetValueAndPriorityLastResult()
                   : Future<void>();
}

Future<void> DisconnectionHandler::UpdateChildren(Variant values) {
  return internal_ ? internal_->UpdateChildren(values) : Future<void>();
}

Future<void> DisconnectionHandler::UpdateChildrenLastResult() {
  return internal_ ? internal_->UpdateChildrenLastResult() : Future<void>();
}

}  // namespace database
}  // namespace firebase
