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

#include "database/src/include/firebase/database/mutable_data.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"

// QueryInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "database/src/android/database_android.h"
#include "database/src/android/mutable_data_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "database/src/ios/database_ios.h"
#include "database/src/ios/mutable_data_ios.h"
#elif defined(FIREBASE_TARGET_DESKTOP)
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/mutable_data_desktop.h"
#else
#include "database/src/stub/database_stub.h"
#include "database/src/stub/mutable_data_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // defined(FIREBASE_TARGET_DESKTOP)

#include "database/src/common/cleanup.h"

namespace firebase {
namespace database {

using internal::MutableDataInternal;

MutableData GetInvalidMutableData() { return MutableData(nullptr); }

typedef CleanupFn<MutableData, MutableDataInternal> CleanupFnMutableData;

template <>
CleanupFnMutableData::CreateInvalidObjectFn
    CleanupFnMutableData::create_invalid_object = GetInvalidMutableData;

MutableData::MutableData(MutableDataInternal* internal) : internal_(internal) {
  CleanupFnMutableData::Register(this, internal_);
}

MutableData::MutableData(const MutableData& rhs)
    : internal_(rhs.internal_ ? rhs.internal_->Clone() : nullptr) {
  CleanupFnMutableData::Register(this, internal_);
}

MutableData& MutableData::operator=(const MutableData& rhs) {
  CleanupFnMutableData::Unregister(this, internal_);
  if (internal_) delete internal_;
  internal_ = rhs.internal_ ? rhs.internal_->Clone() : nullptr;
  CleanupFnMutableData::Register(this, internal_);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)
MutableData::MutableData(MutableData&& rhs) : internal_(rhs.internal_) {
  rhs.internal_ = nullptr;
  CleanupFnMutableData::Unregister(&rhs, internal_);
  CleanupFnMutableData::Register(this, internal_);
}

MutableData& MutableData::operator=(MutableData&& rhs) {
  CleanupFnMutableData::Unregister(this, internal_);
  if (internal_) delete internal_;
  internal_ = rhs.internal_;
  rhs.internal_ = nullptr;
  CleanupFnMutableData::Unregister(&rhs, internal_);
  CleanupFnMutableData::Register(this, internal_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

MutableData::~MutableData() {
  CleanupFnMutableData::Unregister(this, internal_);
  if (internal_) delete internal_;
}

MutableData MutableData::Child(const char* path) {
  return internal_ && path ? MutableData(internal_->Child(path))
                           : GetInvalidMutableData();
}

MutableData MutableData::Child(const std::string& path) {
  return Child(path.c_str());
}

std::vector<MutableData> MutableData::children() {
  return internal_ ? internal_->GetChildren() : std::vector<MutableData>();
}

size_t MutableData::children_count() {
  return internal_ ? internal_->GetChildrenCount() : 0;
}

const char* MutableData::key() const {
  return internal_ ? internal_->GetKey() : "";
}

std::string MutableData::key_string() const {
  return internal_ ? internal_->GetKeyString() : std::string("");
}

Variant MutableData::value() const {
  return internal_ ? internal_->GetValue() : Variant::Null();
}

Variant MutableData::priority() {
  return internal_ ? internal_->GetPriority() : Variant::Null();
}

bool MutableData::HasChild(const char* path) const {
  return internal_ && path ? internal_->HasChild(path) : false;
}

bool MutableData::HasChild(const std::string& path) const {
  return internal_ ? internal_->HasChild(path.c_str()) : false;
}

void MutableData::set_value(const Variant& value) {
  if (internal_) internal_->SetValue(value);
}

void MutableData::set_priority(const Variant& priority) {
  if (internal_) internal_->SetPriority(priority);
}

}  // namespace database
}  // namespace firebase
