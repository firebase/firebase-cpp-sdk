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

namespace firebase {

FutureManager::FutureManager() {}

FutureManager::~FutureManager() {
  {
    MutexLock lock(future_api_mutex_);
    // Move all future APIs to the orphaned list.
    for (auto i = future_apis_.begin(); i != future_apis_.end(); ++i) {
      orphaned_future_apis_.insert(i->second);
    }
    future_apis_.clear();
  }
  CleanupOrphanedFutureApis(/*force_delete_all=*/true);
}

void FutureManager::AllocFutureApi(void* owner, int num_fns) {
  ReferenceCountedFutureImpl* impl = new ReferenceCountedFutureImpl(num_fns);
  InsertFutureApi(owner, impl);
}

void FutureManager::InsertFutureApi(void* owner,
                                    ReferenceCountedFutureImpl* api) {
  bool should_cleanup = false;
  {
    MutexLock lock(future_api_mutex_);
    orphaned_future_apis_.erase(api);
    auto found = future_apis_.find(owner);
    if (found != future_apis_.end()) {
      // Orphan the existing API, and set the new one.
      orphaned_future_apis_.insert(found->second);
      future_apis_[owner] = api;
      should_cleanup = true;
    } else {
      future_apis_.insert(std::make_pair(owner, api));
    }
  }
  if (should_cleanup) {
    CleanupOrphanedFutureApis();
  }
}

void FutureManager::MoveFutureApi(void* prev_owner, void* new_owner) {
  ReferenceCountedFutureImpl* future_api = nullptr;
  {
    MutexLock lock(future_api_mutex_);
    auto found = future_apis_.find(prev_owner);
    if (found != future_apis_.end()) {
      future_api = found->second;
      future_apis_.erase(found);
    }
  }
  if (future_api != nullptr) {
    InsertFutureApi(new_owner, future_api);
  }
}

void FutureManager::ReleaseFutureApi(void* prev_owner) {
  bool should_cleanup = false;
  {
    MutexLock lock(future_api_mutex_);
    auto found = future_apis_.find(prev_owner);
    if (found != future_apis_.end()) {
      // Move the API to the orphaned list.
      orphaned_future_apis_.insert(found->second);
      future_apis_.erase(found);
      should_cleanup = true;
    }
  }
  if (should_cleanup) {
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
  std::vector<ReferenceCountedFutureImpl*> to_delete;
  {
    MutexLock lock(future_api_mutex_);
    for (auto it = orphaned_future_apis_.begin();
         it != orphaned_future_apis_.end();) {
      ReferenceCountedFutureImpl* api = *it;
      if (force_delete_all || IsSafeToDeleteFutureApi(api)) {
        to_delete.push_back(api);
        it = orphaned_future_apis_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Delete outside `future_api_mutex_` so Shutdown() / callback destructors
  // cannot invert lock order with FutureManager. Since
  // ReferenceCountedFutureImpl backs all active callbacks via a shared
  // FutureImplCore, deleting `api` even while a callback is running is safe.
  for (ReferenceCountedFutureImpl* api : to_delete) {
    delete api;
  }
}

bool FutureManager::IsSafeToDeleteFutureApi(ReferenceCountedFutureImpl* api) {
  MutexLock lock(future_api_mutex_);
  return api ? api->IsSafeToDelete() && !api->IsReferencedExternally() : false;
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
