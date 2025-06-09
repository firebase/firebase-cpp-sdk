// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_STORAGE_SRC_ANDROID_LIST_RESULT_ANDROID_H_
#define FIREBASE_STORAGE_SRC_ANDROID_LIST_RESULT_ANDROID_H_

#include <jni.h>

#include <string>
#include <vector>

#include "app/src/util_android.h"
#include "firebase/app.h"
#include "firebase/storage/storage_reference.h"
#include "storage/src/common/storage_internal.h"
// It's okay for platform specific internal headers to include common internal
// headers.
#include "storage/src/common/list_result_internal_common.h"

namespace firebase {
namespace storage {

// Forward declaration for platform-specific ListResultInternal.
class ListResult;

namespace internal {

// Declare ListResultInternal a friend of ListResultInternalCommon for
// construction.
class ListResultInternalCommon;

// Contains the Android-specific implementation of ListResultInternal.
class ListResultInternal {
 public:
  // Constructor.
  //
  // @param[in] storage_internal Pointer to the StorageInternal object.
  // @param[in] java_list_result Java ListResult object. This function will
  // retain a global reference to this object.
  ListResultInternal(StorageInternal* storage_internal,
                     jobject java_list_result);

  // Copy constructor.
  ListResultInternal(const ListResultInternal& other);

  // Copy assignment operator.
  ListResultInternal& operator=(const ListResultInternal& other);

  // Destructor.
  ~ListResultInternal();

  // Gets the items (files) in this result.
  std::vector<StorageReference> items() const;

  // Gets the prefixes (folders) in this result.
  std::vector<StorageReference> prefixes() const;

  // Gets the page token for the next page of results.
  // Returns an empty string if there are no more results.
  std::string page_token() const;

  // Returns the StorageInternal object associated with this ListResult.
  StorageInternal* storage_internal() const { return storage_internal_; }

  // Initializes ListResultInternal JNI.
  static bool Initialize(App* app);

  // Terminates ListResultInternal JNI.
  static void Terminate(App* app);

 private:
  friend class firebase::storage::ListResult;
  // For ListResultInternalCommon's constructor and access to app_ via
  // storage_internal().
  friend class ListResultInternalCommon;

  // Converts a Java List of Java StorageReference objects to a C++ vector of
  // C++ StorageReference objects.
  std::vector<StorageReference> ProcessJavaReferenceList(
      jobject java_list_ref) const;

  StorageInternal* storage_internal_;  // Not owned.
  // Global reference to Java com.google.firebase.storage.ListResult object.
  jobject list_result_java_ref_;

  // Caches for converted data
  mutable std::vector<StorageReference> items_cache_;
  mutable std::vector<StorageReference> prefixes_cache_;
  mutable std::string page_token_cache_;
  mutable bool items_converted_;
  mutable bool prefixes_converted_;
  mutable bool page_token_converted_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_ANDROID_LIST_RESULT_ANDROID_H_
