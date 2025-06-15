#include "storage/src/desktop/list_result_desktop.h"

namespace firebase {
namespace storage {
namespace internal {

ListResultInternalDesktop::ListResultInternalDesktop()
    : items_stub_(), page_token_stub_() {}

ListResultInternalDesktop::ListResultInternalDesktop(const ListResultInternalDesktop& other)
    : items_stub_(other.items_stub_), page_token_stub_(other.page_token_stub_) {}

ListResultInternalDesktop& ListResultInternalDesktop::operator=(
    const ListResultInternalDesktop& other) {
  if (this == &other) {
    return *this;
  }
  items_stub_ = other.items_stub_;
  page_token_stub_ = other.page_token_stub_;
  return *this;
}

ListResultInternalDesktop::ListResultInternalDesktop(ListResultInternalDesktop&& other) noexcept
    : items_stub_(std::move(other.items_stub_)),
      page_token_stub_(std::move(other.page_token_stub_)) {}

ListResultInternalDesktop& ListResultInternalDesktop::operator=(
    ListResultInternalDesktop&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  items_stub_ = std::move(other.items_stub_);
  page_token_stub_ = std::move(other.page_token_stub_);
  return *this;
}

const std::vector<StorageReference>& ListResultInternalDesktop::items() const {
  return items_stub_; // Always empty
}

const std::string& ListResultInternalDesktop::page_token() const {
  return page_token_stub_; // Always empty
}

bool ListResultInternalDesktop::is_valid() const {
  // A stub ListResult can be considered "valid" in the sense that it's
  // initialized, even if it's empty and represents an unimplemented feature's result.
  // However, since StorageReference::List/ListAll will complete with an error,
  // this ListResult might not be used with a successful future.
  // If it were to be used with a successful future, returning true would be appropriate.
  // For consistency with how an error future is typically handled (where the result
  // might be default-constructed but ignored), `true` is acceptable.
  // Alternatively, `false` could indicate it holds no meaningful data.
  // Let's choose `true` for now, assuming the ListResult itself is constructed correctly.
  return true;
}

ListResultInternal* ListResultInternalDesktop::Clone() {
  return new ListResultInternalDesktop(*this);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
