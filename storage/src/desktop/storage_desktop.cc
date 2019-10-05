// Copyright 2017 Google LLC
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

#include "storage/src/desktop/storage_desktop.h"

#include <assert.h>

#include <algorithm>

#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/desktop/storage_reference_desktop.h"

namespace firebase {
namespace storage {
namespace internal {

StorageInternal::StorageInternal(App* app, const char* url) {
  app_ = app;

  if (url) {
    url_ = url;
    root_ = StoragePath(url_);
  } else {
    const char* bucket = app->options().storage_bucket();
    root_ = StoragePath(bucket ? std::string(kGsScheme) + bucket : "");
  }

  // LINT.IfChange
  max_download_retry_time_ = 600.0;
  max_operation_retry_time_ = 120.0;
  max_upload_retry_time_ = 600.0;
  // LINT.ThenChange(//depot/google3/java/com/google/android/gmscore/integ/\
  //            client/firebase-storage-api/src/com/google/firebase/\
  //            storage/FirebaseStorage.java,
  //         //depot_firebase_ios_Releases/FirebaseStorage/\
  //            Library/FIRStorage.m)

  firebase::rest::util::Initialize();
  firebase::rest::InitTransportCurl();
  // Spin up the token auto-update thread in Auth.
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthStartTokenListener, app_, nullptr, nullptr);

  std::string sdk;
  std::string version;
  app_common::GetOuterMostSdkAndVersion(&sdk, &version);
  assert(!sdk.empty() && !version.empty());
  user_agent_ =
      sdk.substr(sizeof(FIREBASE_USER_AGENT_PREFIX) - 1) + "/" + version;
}

StorageInternal::~StorageInternal() {
  cleanup().CleanupAll();
  firebase::rest::CleanupTransportCurl();
  firebase::rest::util::Terminate();
  // Stop the token auto-update thread in Auth.
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthStopTokenListener, app_, nullptr, nullptr);
  app_ = nullptr;
}

// Get a StorageReference to the root of the database.
StorageReferenceInternal* StorageInternal::GetReference() const {
  return new StorageReferenceInternal(url_, const_cast<StorageInternal*>(this));
}

// Get a StorageReference for the specified path.
StorageReferenceInternal* StorageInternal::GetReference(
    const char* path) const {
  return new StorageReferenceInternal(root_.GetChild(path),
                                      const_cast<StorageInternal*>(this));
}

// Get a StorageReference for the provided URL.
StorageReferenceInternal* StorageInternal::GetReferenceFromUrl(
    const char* url) const {
  return new StorageReferenceInternal(url, const_cast<StorageInternal*>(this));
}

// Returns the auth token for the current user, if there is a current user,
// and they have a token, and auth exists as part of the app.
// Otherwise, returns an empty string.
std::string StorageInternal::GetAuthToken() {
  std::string result;
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetCurrentToken, app(), nullptr, &result);
  return result;
}

// Add an operation to the list of outstanding operations.
void StorageInternal::AddOperation(RestOperation* operation) {
  MutexLock lock(operations_mutex_);
  CleanupCompletedOperations();
  RemoveOperation(operation);
  operations_.push_back(operation);
}

// Remove an operation from the list of outstanding operations.
void StorageInternal::RemoveOperation(RestOperation* operation) {
  MutexLock lock(operations_mutex_);
  auto it = std::find(operations_.begin(), operations_.end(), operation);
  if (it != operations_.end()) {
    operations_.erase(it);
  }
}

// Clean up completed operations.
void StorageInternal::CleanupCompletedOperations() {
  MutexLock lock(operations_mutex_);
  std::vector<RestOperation*> operations_to_delete;
  for (auto it = operations_.begin(); it != operations_.end(); ++it) {
    if ((*it)->is_complete()) operations_to_delete.push_back(*it);
  }
  for (auto it = operations_to_delete.begin(); it != operations_to_delete.end();
       ++it) {
    RemoveOperation(*it);
    delete *it;
  }
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
