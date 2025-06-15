#include "firebase/storage/list_result.h"
#include "storage/src/common/list_result_internal.h" // For ListResultInternal
#include "app/src/assert.h" // For FIREBASE_ASSERT

namespace firebase {
namespace storage {

// Initialize static empty members
const std::vector<StorageReference> ListResult::s_empty_items_;
const std::vector<StorageReference> ListResult::s_empty_prefixes_;
const std::string ListResult::s_empty_page_token_;

ListResult::ListResult() : internal_() {}

ListResult::ListResult(internal::ListResultInternal* internal_list_result)
    : internal_(internal_list_result) {
  // InternalUniquePtr will take ownership and manage cleanup via CleanupNotifier
  // associated with internal_list_result's StorageReferenceInternal.
}

ListResult::ListResult(const ListResult& other) : internal_() {
  if (other.internal_) {
    // We need to pass the StorageReferenceInternal associated with the *new* ListResult.
    // However, the public ListResult doesn't directly own a StorageReferenceInternal.
    // The ListResultInternal does. The Clone method takes the *new parent's* SRI.
    // This implies that the public ListResult needs access to its future parent's SRI
    // or the Clone method on ListResultInternal needs to be smarter, or
    // the SRI passed to clone is the one from other.internal_->storage_reference_internal_.
    // Let's assume Clone duplicates with the same SRI association initially,
    // as the ListResult object itself doesn't independently manage an SRI.
    // The SRI in ListResultInternal is for cleanup purposes.
    // A cloned ListResultInternal should probably share the same SRI context initially
    // if it's just a data snapshot.
    // The Clone signature is `ListResultInternal* Clone(StorageReferenceInternal* new_parent_sri) const`.
    // This means `other.internal_->storage_reference_internal_` is the one to pass.
    internal_.reset(other.internal_->Clone(other.internal_->storage_reference_internal_));
  }
}

ListResult::ListResult(ListResult&& other) : internal_(std::move(other.internal_)) {}

ListResult::~ListResult() {}

ListResult& ListResult::operator=(const ListResult& other) {
  if (this == &other) {
    return *this;
  }
  if (other.internal_) {
    // Same logic as copy constructor for cloning.
    // Pass the SRI from the source of the clone.
    internal_.reset(other.internal_->Clone(other.internal_->storage_reference_internal_));
  } else {
    internal_.reset(nullptr); // Assigning from an invalid ListResult
  }
  return *this;
}

ListResult& ListResult::operator=(ListResult&& other) {
  if (this == &other) {
    return *this;
  }
  internal_ = std::move(other.internal_);
  return *this;
}

const std::vector<StorageReference>& ListResult::items() const {
  if (!is_valid()) {
    return s_empty_items_;
  }
  return internal_->items();
}

const std::vector<StorageReference>& ListResult::prefixes() const {
  if (!is_valid()) {
    return s_empty_prefixes_;
  }
  return internal_->prefixes();
}

const std::string& ListResult::page_token() const {
  if (!is_valid()) {
    return s_empty_page_token_;
  }
  return internal_->page_token();
}

bool ListResult::is_valid() const { return internal_ != nullptr; }

}  // namespace storage
}  // namespace firebase
