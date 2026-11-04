// Copyright 2020 Google LLC
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
#endif

namespace firebase::firestore {
namespace {
using CleanupFnWriteBatch = CleanupFn<WriteBatch>;
void ValidateReference(const DocumentReference& document) {
  if (!document.is_valid()) SimpleThrowInvalidArgument("Invalid document reference provided.");
}
}  // namespace

WriteBatch::WriteBatch() {}

WriteBatch::WriteBatch(const WriteBatch& value)
    : internal_(value.internal_ ? new WriteBatchInternal(*value.internal_) : nullptr) {
  CleanupFnWriteBatch::Register(this, internal_);
}

WriteBatch::WriteBatch(WriteBatch&& value) noexcept
    : internal_(std::exchange(value.internal_, nullptr)) {
  CleanupFnWriteBatch::Unregister(&value, internal_);
  CleanupFnWriteBatch::Register(this, internal_);
}

WriteBatch::WriteBatch(WriteBatchInternal* internal) : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  CleanupFnWriteBatch::Register(this, internal_);
}

WriteBatch::~WriteBatch() {
  CleanupFnWriteBatch::Unregister(this, internal_);
  delete std::exchange(internal_, nullptr);
}

WriteBatch& WriteBatch::operator=(const WriteBatch& value) {
  if (this == &value) return *this;
  WriteBatch tmp(value);
  return *this = std::move(tmp);
}

WriteBatch& WriteBatch::operator=(WriteBatch&& value) noexcept {
  if (this == &value) return *this;
  CleanupFnWriteBatch::Unregister(&value, value.internal_);
  CleanupFnWriteBatch::Unregister(this, internal_);
  delete std::exchange(internal_, std::exchange(value.internal_, nullptr));
  CleanupFnWriteBatch::Register(this, internal_);
  return *this;
}

WriteBatch& WriteBatch::Set(const DocumentReference& document, const MapFieldValue& data,
                            const SetOptions& options) {
  if (!internal_) return *this;
  ValidateReference(document);
  internal_->Set(document, data, options);
  return *this;
}

WriteBatch& WriteBatch::Update(const DocumentReference& document, const MapFieldValue& data) {
  if (!internal_) return *this;
  ValidateReference(document);
  internal_->Update(document, data);
  return *this;
}

WriteBatch& WriteBatch::Update(const DocumentReference& document, const MapFieldPathValue& data) {
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
  return internal_ ? internal_->Commit() : FailedFuture<void>();
}

}  // namespace firebase::firestore
