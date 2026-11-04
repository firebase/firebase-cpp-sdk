// Copyright 2019 Google LLC
#include "firebase/storage/list_result.h"
#include "list_result_internal.h"

namespace firebase::storage {

StorageListResult::StorageListResult() : internal_(nullptr) {}
StorageListResult::~StorageListResult() { delete internal_; }

StorageListResult::StorageListResult(const StorageListResult& other)
    : internal_(other.internal_
                    ? new internal::StorageListResultInternal(*other.internal_)
                    : nullptr) {}

StorageListResult& StorageListResult::operator=(
    const StorageListResult& other) {
  if (this != &other) {
    delete internal_;
    internal_ = other.internal_
                    ? new internal::StorageListResultInternal(*other.internal_)
                    : nullptr;
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
StorageListResult::StorageListResult(StorageListResult&& other)
    : internal_(other.internal_) {
  other.internal_ = nullptr;
}

StorageListResult& StorageListResult::operator=(StorageListResult&& other) {
  if (this != &other) {
    delete internal_;
    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}
#endif

const std::vector<StorageReference>& StorageListResult::prefixes() const {
  static const std::vector<StorageReference> kEmpty;
  return internal_ ? internal_->prefixes_ : kEmpty;
}

const std::vector<StorageReference>& StorageListResult::items() const {
  static const std::vector<StorageReference> kEmpty;
  return internal_ ? internal_->items_ : kEmpty;
}

const std::string& StorageListResult::next_page_token() const {
  static const std::string kEmpty;
  return internal_ ? internal_->next_page_token_ : kEmpty;
}

bool StorageListResult::is_valid() const { return internal_ != nullptr; }

std::string StorageListResult::ToString() const {
  if (!is_valid()) return "StorageListResult(invalid)";
  return "StorageListResult(prefixes=" + std::to_string(prefixes().size()) +
         ", items=" + std::to_string(items().size()) +
         ", next_page_token=\"" + next_page_token() + "\")";
}

bool StorageListResult::operator==(const StorageListResult& other) const {
  if (is_valid() != other.is_valid()) return false;
  if (!is_valid()) return true;
  return prefixes() == other.prefixes() && items() == other.items() &&
         next_page_token() == other.next_page_token();
}

}  // namespace firebase::storage
