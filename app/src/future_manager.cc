/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/src/future_manager.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

FutureManager::FutureManager() {}

FutureManager::~FutureManager() {
  MutexLock lock(future_api_mutex_);
  // Move all future APIs to the orphaned list.
  for (auto i = future_apis_.begin(); i != future_apis_.end(); ++i) {
    orphaned_future_apis_.insert(i->second);
  }
  future_apis_.clear();
  CleanupOrphanedFutureApis(true);  // force delete all
}

void FutureManager::AllocFutureApi(void* owner, int num_fns) {
  MutexLock lock(future_api_mutex_);

  ReferenceCountedFutureImpl* impl = new ReferenceCountedFutureImpl(num_fns);
  InsertFutureApi(owner, impl);
}

void FutureManager::InsertFutureApi(void* owner,
                                    ReferenceCountedFutureImpl* api) {
  MutexLock lock(future_api_mutex_);
  orphaned_future_apis_.erase(api);
  auto found = future_apis_.find(owner);
  if (found != future_apis_.end()) {
    // Orphan the existing API, and set the new one.
    orphaned_future_apis_.insert(found->second);
    future_apis_[owner] = api;
    CleanupOrphanedFutureApis();
  } else {
    future_apis_.insert(std::make_pair(owner, api));
  }
}

void FutureManager::MoveFutureApi(void* prev_owner, void* new_owner) {
  MutexLock lock(future_api_mutex_);

  auto found = future_apis_.find(prev_owner);
  if (found != future_apis_.end()) {
    ReferenceCountedFutureImpl* future_api = found->second;
    future_apis_.erase(found);
    InsertFutureApi(new_owner, future_api);
  }
}

void FutureManager::ReleaseFutureApi(void* prev_owner) {
  MutexLock lock(future_api_mutex_);

  auto found = future_apis_.find(prev_owner);
  if (found != future_apis_.end()) {
    // Move the API to the orphaned list.
    orphaned_future_apis_.insert(found->second);
    future_apis_.erase(found);
    CleanupOrphanedFutureApis();
  }
}

ReferenceCountedFutureImpl* FutureManager::GetFutureApi(void* owner) {
  MutexLock lock(future_api_mutex_);

  auto found = future_apis_.find(owner);
  if (found != future_apis_.end()) {
    return found->second;
  } else {
    return nullptr;
  }
}

void FutureManager::CleanupOrphanedFutureApis(bool force_delete_all) {
  MutexLock lock(future_api_mutex_);
  std::vector<ReferenceCountedFutureImpl*> to_delete;
  for (auto api = orphaned_future_apis_.begin();
       api != orphaned_future_apis_.end(); ++api) {
    if (force_delete_all || IsSafeToDeleteFutureApi(*api)) {
      to_delete.push_back(*api);
    }
  }

  for (int i = 0; i < to_delete.size(); i++) {
    auto* api = to_delete[i];
    // Deleting the API could result in the side effect of running
    // CleanupOrphanedFutureApis() again so make sure the API we're going to
    // delete is still in the orphan list.
    orphaned_future_apis_.erase(api);
    api->cleanup().cleanup_notifier().RegisterObject(
        &to_delete[i], [](void* object) {
          *(reinterpret_cast<ReferenceCountedFutureImpl**>(object)) = nullptr;
        });
  }

  for (int i = 0; i < to_delete.size(); i++) {
    auto* api = to_delete[i];
    if (api) delete api;
  }
}

bool FutureManager::IsSafeToDeleteFutureApi(ReferenceCountedFutureImpl* api) {
  MutexLock lock(future_api_mutex_);
  return api ? api->IsSafeToDelete() && !api->IsReferencedExternally()
             : false;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
