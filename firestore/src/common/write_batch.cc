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

#include "firestore/src/include/firebase/firestore/write_batch.h"

#include <utility>

#include "app/src/include/firebase/future.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#if defined(__ANDROID__)
#include "firestore/src/android/write_batch_android.h"
#else
#include "firestore/src/main/write_batch_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

namespace {

using CleanupFnWriteBatch = CleanupFn<WriteBatch>;

void ValidateReference(const DocumentReference& document) {
  if (!document.is_valid()) {
    SimpleThrowInvalidArgument("Invalid document reference provided.");
  }
}

}  // namespace

WriteBatch::WriteBatch() {}

WriteBatch::WriteBatch(const WriteBatch& value) {
  if (value.internal_) {
    internal_ = new WriteBatchInternal(*value.internal_);
  }
  CleanupFnWriteBatch::Register(this, internal_);
}

WriteBatch::WriteBatch(WriteBatch&& value) {
  CleanupFnWriteBatch::Unregister(&value, value.internal_);
  std::swap(internal_, value.internal_);
  CleanupFnWriteBatch::Register(this, internal_);
}

WriteBatch::WriteBatch(WriteBatchInternal* internal) : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  CleanupFnWriteBatch::Register(this, internal_);
}

WriteBatch::~WriteBatch() {
  CleanupFnWriteBatch::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

WriteBatch& WriteBatch::operator=(const WriteBatch& value) {
  if (this == &value) {
    return *this;
  }

  CleanupFnWriteBatch::Unregister(this, internal_);
  delete internal_;
  if (value.internal_) {
    internal_ = new WriteBatchInternal(*value.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnWriteBatch::Register(this, internal_);
  return *this;
}

WriteBatch& WriteBatch::operator=(WriteBatch&& value) {
  if (this == &value) {
    return *this;
  }

  CleanupFnWriteBatch::Unregister(&value, value.internal_);
  CleanupFnWriteBatch::Unregister(this, internal_);
  delete internal_;
  internal_ = value.internal_;
  value.internal_ = nullptr;
  CleanupFnWriteBatch::Register(this, internal_);
  return *this;
}

WriteBatch& WriteBatch::Set(const DocumentReference& document,
                            const MapFieldValue& data,
                            const SetOptions& options) {
  if (!internal_) return *this;

  ValidateReference(document);
  internal_->Set(document, data, options);
  return *this;
}

WriteBatch& WriteBatch::Update(const DocumentReference& document,
                               const MapFieldValue& data) {
  if (!internal_) return *this;

  ValidateReference(document);
  internal_->Update(document, data);
  return *this;
}

WriteBatch& WriteBatch::Update(const DocumentReference& document,
                               const MapFieldPathValue& data) {
  if (!internal_) return *this;

  ValidateReference(document);
  internal_->Update(document, data);
  return *this;
}

WriteBatch& WriteBatch::Delete(const DocumentReference& document) {
  if (!internal_) return *this;

  ValidateReference(document);
  internal_->Delete(document);
  return *this;
}

Future<void> WriteBatch::Commit() {
  if (!internal_) return FailedFuture<void>();
  return internal_->Commit();
}

}  // namespace firestore
}  // namespace firebase
