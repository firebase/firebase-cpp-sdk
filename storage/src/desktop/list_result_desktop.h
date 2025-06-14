#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

#include <string>
#include <vector>
#include "firebase/storage/list_result.h" // For ListResultInternal and StorageReference

// Forward declaration (if not already via ListResultInternal)
// namespace firebase { namespace storage { class StorageReference; } }

class ListResultInternalDesktop : public ListResultInternal {
 public:
  ListResultInternalDesktop(); // Default constructor
  ~ListResultInternalDesktop() override = default;

  // Copy constructor and assignment
  ListResultInternalDesktop(const ListResultInternalDesktop& other);
  ListResultInternalDesktop& operator=(const ListResultInternalDesktop& other);

  // Move constructor and assignment (optional, but good practice)
  ListResultInternalDesktop(ListResultInternalDesktop&& other) noexcept;
  ListResultInternalDesktop& operator=(ListResultInternalDesktop&& other) noexcept;

  const std::vector<StorageReference>& items() const override;
  const std::string& page_token() const override;
  bool is_valid() const override;
  ListResultInternal* Clone() override;

 private:
  std::vector<StorageReference> items_stub_; // Always empty
  std::string page_token_stub_;      // Always empty
};

// Make ListResultInternal an alias for ListResultInternalDesktop on Desktop.
typedef ListResultInternalDesktop ListResultInternal;

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_
