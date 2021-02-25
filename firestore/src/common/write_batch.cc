#include "firestore/src/include/firebase/firestore/write_batch.h"

#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/futures.h"

#include "firestore/src/include/firebase/firestore/document_reference.h"
#if defined(__ANDROID__)
#include "firestore/src/android/write_batch_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/write_batch_stub.h"
#else
#include "firestore/src/ios/write_batch_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnWriteBatch = CleanupFn<WriteBatch>;

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
  FIREBASE_ASSERT(internal != nullptr);
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
  internal_->Set(document, data, options);
  return *this;
}

WriteBatch& WriteBatch::Update(const DocumentReference& document,
                               const MapFieldValue& data) {
  if (!internal_) return *this;
  internal_->Update(document, data);
  return *this;
}

WriteBatch& WriteBatch::Update(const DocumentReference& document,
                               const MapFieldPathValue& data) {
  if (!internal_) return *this;
  internal_->Update(document, data);
  return *this;
}

WriteBatch& WriteBatch::Delete(const DocumentReference& document) {
  if (!internal_) return *this;
  internal_->Delete(document);
  return *this;
}

Future<void> WriteBatch::Commit() {
  if (!internal_) return FailedFuture<void>();
  return internal_->Commit();
}

}  // namespace firestore
}  // namespace firebase
