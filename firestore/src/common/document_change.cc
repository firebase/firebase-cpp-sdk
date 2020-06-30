#include "firestore/src/include/firebase/firestore/document_change.h"

#include <utility>

#include "app/src/assert.h"
#include "firestore/src/common/cleanup.h"

#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_change_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/document_change_stub.h"
#else
#include "firestore/src/ios/document_change_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnDocumentChange = CleanupFn<DocumentChange>;
using Type = DocumentChange::Type;

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
  FIREBASE_ASSERT(internal != nullptr);
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

}  // namespace firestore
}  // namespace firebase
