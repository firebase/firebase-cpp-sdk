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

#include <vector>

namespace firebase {

FutureManager::FutureManager() = default;

FutureManager::~FutureManager() {
  {
    MutexLock lock(future_api_mutex_);
    // Move all future APIs to the orphaned list.
    for (const auto& entry : future_apis_) {
      orphaned_future_apis_.insert(entry.second);
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
      if (found->second != api) {
        // Orphan the existing API, and set the new one.
        orphaned_future_apis_.insert(found->second);
        found->second = api;
        should_cleanup = true;
      }
    } else {
      future_apis_.emplace(owner, api);
    }
  }
  if (should_cleanup) {
    CleanupOrphanedFutureApis();
  }
}

void FutureManager::MoveFutureApi(void* prev_owner, void* new_owner) {
  if (prev_owner == new_owner) {
    return;
  }
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
  } else {
    ReleaseFutureApi(new_owner);
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
    for (ReferenceCountedFutureImpl* api : orphaned_future_apis_) {
      if (api == nullptr) {
        continue;
      }
      if (api->IsSafeToDelete() && !api->IsReferencedExternally()) {
        to_delete.push_back(api);
      } else if (force_delete_all) {
        // Deleting an API while it's running a callback (which could have
        // triggered the current call to `CleanupOrphanedFutureApis`) will lead
        // to a use-after-free. Instead, mark the API to be deleted once the
        // callback finishes.
        if (api->IsRunningCallback()) {
          api->MarkOrphaned();
        } else {
          to_delete.push_back(api);
        }
      }
    }

    for (ReferenceCountedFutureImpl* api : to_delete) {
      orphaned_future_apis_.erase(api);
    }
  }

  for (ReferenceCountedFutureImpl* api : to_delete) {
    delete api;
  }
}

bool FutureManager::IsSafeToDeleteFutureApi(ReferenceCountedFutureImpl* api) {
  MutexLock lock(future_api_mutex_);
  return api ? api->IsSafeToDelete() && !api->IsReferencedExternally() : false;
}

}  // namespace firebase
