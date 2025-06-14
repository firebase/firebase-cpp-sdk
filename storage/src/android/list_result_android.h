#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_LIST_RESULT_ANDROID_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_LIST_RESULT_ANDROID_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {


#include "app/src/util_android.h"
#include "firebase/app.h"
#include "firebase/storage/list_result.h"
#include "storage/src/android/storage_android.h" // For StorageInternal

// Forward declaration
namespace firebase {
namespace storage {
class StorageReference;
}
}

class ListResultInternalAndroid : public ListResultInternal {
 public:
  ListResultInternalAndroid(StorageInternal* storage_internal, jobject java_list_result);
  ~ListResultInternalAndroid() override;

  // Disable copy and move constructors/assignments
  ListResultInternalAndroid(const ListResultInternalAndroid&) = delete;
  ListResultInternalAndroid& operator=(const ListResultInternalAndroid&) = delete;
  ListResultInternalAndroid(ListResultInternalAndroid&&) = delete;
  ListResultInternalAndroid& operator=(ListResultInternalAndroid&&) = delete;

  const std::vector<StorageReference>& items() const override;
  const std::string& page_token() const override;
  bool is_valid() const override;
  ListResultInternal* Clone() override;

 private:
  StorageInternal* storage_internal_; // Needed to create C++ StorageReference objects
  jobject java_list_result_global_ref_;
  // Cached C++ versions
  mutable std::vector<StorageReference> items_cpp_;
  mutable std::string page_token_cpp_;
  mutable bool items_converted_;
  mutable bool page_token_converted_;
};

// Make ListResultInternal an alias for ListResultInternalAndroid on Android.
typedef ListResultInternalAndroid ListResultInternal;

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_LIST_RESULT_ANDROID_H_
