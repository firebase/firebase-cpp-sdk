#include "firestore/src/main/document_change_main.h"

#include <utility>

#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/document_snapshot_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase {
namespace firestore {

using Type = DocumentChange::Type;

DocumentChangeInternal::DocumentChangeInternal(api::DocumentChange&& change)
    : change_{std::move(change)} {}

FirestoreInternal* DocumentChangeInternal::firestore_internal() {
  return GetFirestoreInternal(&change_);
}

Type DocumentChangeInternal::type() const {
  switch (change_.type()) {
    case api::DocumentChange::Type::Added:
      return Type::kAdded;
    case api::DocumentChange::Type::Modified:
      return Type::kModified;
    case api::DocumentChange::Type::Removed:
      return Type::kRemoved;
  }
  FIRESTORE_UNREACHABLE();
}

DocumentSnapshot DocumentChangeInternal::document() const {
  return MakePublic(change_.document());
}

std::size_t DocumentChangeInternal::old_index() const {
  return change_.old_index();
}

std::size_t DocumentChangeInternal::new_index() const {
  return change_.new_index();
}

}  // namespace firestore
}  // namespace firebase
