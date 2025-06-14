#include "firebase/storage/list_result.h"

#include "app/src/include/firebase/app.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/storage/common/internal/list_result_internal.h"
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/list_result_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/list_result_ios.h"
#else
#include "storage/src/desktop/list_result_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace storage {

// Constructor that takes items and page_token.
// This constructor is not expected to be used directly by users.
// It's used internally or for testing.
ListResult::ListResult(std::vector<StorageReference> items,
                       std::string page_token)
    : items_(std::move(items)),
      page_token_(std::move(page_token)),
      internal_(nullptr) {}

ListResult::ListResult(internal::ListResultInternal* internal)
    : internal_(internal) {}

ListResult::ListResult(const ListResult& other) : internal_(nullptr) {
  if (other.internal_) {
    internal_ = other.internal_->Clone();
  } else {
    // If other.internal_ is null, it means other was likely created
    // using the (items, page_token) constructor. So, copy those members.
    items_ = other.items_;
    page_token_ = other.page_token_;
  }
}

ListResult& ListResult::operator=(const ListResult& other) {
  if (this == &other) {
    return *this;
  }
  delete internal_;
  if (other.internal_) {
    internal_ = other.internal_->Clone();
  } else {
    internal_ = nullptr;
    // If other.internal_ is null, copy its members.
    items_ = other.items_;
    page_token_ = other.page_token_;
  }
  return *this;
}

ListResult::ListResult(ListResult&& other) : internal_(other.internal_) {
  other.internal_ = nullptr;
  if (internal_ == nullptr) {
    items_ = std::move(other.items_);
    page_token_ = std::move(other.page_token_);
  }
}

ListResult& ListResult::operator=(ListResult&& other) {
  if (this == &other) {
    return *this;
  }
  delete internal_;
  internal_ = other.internal_;
  other.internal_ = nullptr;
  if (internal_ == nullptr) {
    items_ = std::move(other.items_);
    page_token_ = std::move(other.page_token_);
  }
  return *this;
}

ListResult::~ListResult() { delete internal_; }

const std::vector<StorageReference>& ListResult::items() const {
  if (internal_) {
    return internal_->items();
  }
  return items_;
}

const std::string& ListResult::page_token() const {
  if (internal_) {
    return internal_->page_token();
  }
  return page_token_;
}

bool ListResult::is_valid() const {
  // If internal_ is null, it means it was default constructed or constructed
  // with items and page_token, which is valid in those contexts.
  // If internal_ is not null, then its validity determines the ListResult's validity.
  return internal_ ? internal_->is_valid() : true;
}

namespace internal {

// Define a ListResultInternalCommon for cleanup if needed, though for
// ListResult, the platform-specific internal objects might handle all
// resources. If common resources are needed, this is where they'd be managed.
// For now, it's a placeholder.
class ListResultInternalCommon : public ListResultInternal {
 public:
  ~ListResultInternalCommon() override = default;

  const std::vector<StorageReference>& items() const override {
    // This should be implemented by a concrete platform implementation
    // or by holding items directly if there's a common representation.
    static std::vector<StorageReference> empty_items;
    return empty_items;
  }

  const std::string& page_token() const override {
    // Similarly, implemented by platform or common storage.
    static std::string empty_token;
    return empty_token;
  }

  bool is_valid() const override {
    // Common validity logic, if any.
    return false;
  }
};

// The static ListResultInternal::Clone method is no longer needed and should be removed.
// The ListResultInternalCommon class and its methods are placeholders and might
// need further implementation or removal if ListResultInternal instances are
// always platform-specific.

}  // namespace internal
}  // namespace storage
}  // namespace firebase
