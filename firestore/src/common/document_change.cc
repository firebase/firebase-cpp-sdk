/*
 * Copyright 2020 Google LLC
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

#include "firestore/src/include/firebase/firestore/document_change.h"

#include <utility>

#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_change_android.h"
#else
#include "firestore/src/main/document_change_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnDocumentChange = CleanupFn<DocumentChange>;
using Type = DocumentChange::Type;

#if defined(ANDROID)
// Older NDK (r16b) fails to define this properly. Fix this when support for
// the older NDK is removed.
const std::size_t DocumentChange::npos = static_cast<std::size_t>(-1);
#else
constexpr std::size_t DocumentChange::npos;
#endif  // defined(ANDROID)

DocumentChange::DocumentChange() {}

DocumentChange::DocumentChange(const DocumentChange& value) {
  if (value.internal_) {
    internal_ = new DocumentChangeInternal(*value.internal_);
  }
  CleanupFnDocumentChange::Register(this, internal_);
}

DocumentChange::DocumentChange(DocumentChange&& value) {
  CleanupFnDocumentChange::Unregister(&value, value.internal_);
  std::swap(internal_, value.internal_);
  CleanupFnDocumentChange::Register(this, internal_);
}

DocumentChange::DocumentChange(DocumentChangeInternal* internal)
    : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  CleanupFnDocumentChange::Register(this, internal_);
}

DocumentChange::~DocumentChange() {
  CleanupFnDocumentChange::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

DocumentChange& DocumentChange::operator=(const DocumentChange& value) {
  if (this == &value) {
    return *this;
  }

  CleanupFnDocumentChange::Unregister(this, internal_);
  delete internal_;
  if (value.internal_) {
    internal_ = new DocumentChangeInternal(*value.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnDocumentChange::Register(this, internal_);
  return *this;
}

DocumentChange& DocumentChange::operator=(DocumentChange&& value) {
  if (this == &value) {
    return *this;
  }

  CleanupFnDocumentChange::Unregister(&value, value.internal_);
  CleanupFnDocumentChange::Unregister(this, internal_);
  delete internal_;
  internal_ = value.internal_;
  value.internal_ = nullptr;
  CleanupFnDocumentChange::Register(this, internal_);
  return *this;
}

Type DocumentChange::type() const {
  if (!internal_) return {};
  return internal_->type();
}

DocumentSnapshot DocumentChange::document() const {
  if (!internal_) return {};
  return internal_->document();
}

std::size_t DocumentChange::old_index() const {
  if (!internal_) return {};
  return internal_->old_index();
}

std::size_t DocumentChange::new_index() const {
  if (!internal_) return {};
  return internal_->new_index();
}

size_t DocumentChange::Hash() const {
  if (!internal_) return {};
  return internal_->Hash();
}

bool operator==(const DocumentChange& lhs, const DocumentChange& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

}  // namespace firestore
}  // namespace firebase
