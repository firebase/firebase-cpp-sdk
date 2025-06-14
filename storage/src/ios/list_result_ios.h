#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

#include "firebase/app.h"
#include "firebase/storage/list_result.h" // For ListResultInternal
#include "storage/src/ios/storage_ios.h"   // For StorageInternal
#include "app/src/util_ios.h"      // For OBJ_C_PTR_WRAPPER if needed for FIRStorageListResult

// Forward declaration
namespace firebase {
namespace storage {
class StorageReference;
}
}

// Forward declare Obj-C class
#ifdef __OBJC__
@class FIRStorageListResult;
#else
class FIRStorageListResult;
#endif

OBJ_C_PTR_WRAPPER(FIRStorageListResult);

class ListResultInternalIOS : public ListResultInternal {
 public:
  ListResultInternalIOS(StorageInternal* storage_internal,
                        FIRStorageListResult* list_result_objc); // Takes ownership via strong reference
  ~ListResultInternalIOS() override;

  // Disable copy and move constructors/assignments initially
  ListResultInternalIOS(const ListResultInternalIOS&) = delete;
  ListResultInternalIOS& operator=(const ListResultInternalIOS&) = delete;
  ListResultInternalIOS(ListResultInternalIOS&&) = delete;
  ListResultInternalIOS& operator=(ListResultInternalIOS&&) = delete;

  const std::vector<StorageReference>& items() const override;
  const std::string& page_token() const override;
  bool is_valid() const override;
  ListResultInternal* Clone() override; // Should use [FIRStorageListResult copy] if available and appropriate

 private:
  StorageInternal* storage_internal_; // Needed to create C++ StorageReference objects
  std::unique_ptr<FIRStorageListResultPointer> list_result_objc_ptr_;

  // Cached C++ versions
  mutable std::vector<StorageReference> items_cpp_;
  mutable std::string page_token_cpp_;
  mutable bool items_converted_;
  mutable bool page_token_converted_;
};

// Make ListResultInternal an alias for ListResultInternalIOS on iOS.
typedef ListResultInternalIOS ListResultInternal;

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_
