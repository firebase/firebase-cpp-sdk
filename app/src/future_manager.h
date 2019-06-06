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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_FUTURE_MANAGER_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_FUTURE_MANAGER_H_

#include <map>
#include <set>
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

#ifdef USE_PLAYBILLING_FUTURE
#include "playbillingclient/future.h"
#else
#include "app/src/include/firebase/future.h"
#endif

namespace FIREBASE_NAMESPACE {

// Class for handling Future APIs. These all work directly with
// ReferenceCountedFutureImpl, as that is the Future API we use.
class FutureManager {
 public:
  FutureManager();
  ~FutureManager();

  // Allocate a ReferenceCountedFutureImpl for this object to use.
  void AllocFutureApi(void* owner, int num_fns);

  // Change the owner of an existing ReferenceCountedFutureImpl to a new object.
  // Used by move constructors. If the previous owner did not have a
  // ReferenceCountedFutureImpl registered to it, this does nothing.
  void MoveFutureApi(void* prev_owner, void* new_owner);

  // Release a ReferenceCountedFutureImpl. If any Futures are still active, it
  // will go into the orphaned list.
  void ReleaseFutureApi(void* prev_owner);

  // Get the ReferenceCountedFutureImpl for a given object.
  ReferenceCountedFutureImpl* GetFutureApi(void* owner);

  // Check all orphaned ReferenceCountedFutureImpl. For each one, if it has no
  // pending futures, and no external references to any futures, it's safe to
  // clean up, and will be deleted.
  //
  // If force_delete_all is true, will delete all ReferenceCountedFutureImpl in
  // the orphaned list. Used by the FutureManager's destructor.
  void CleanupOrphanedFutureApis(bool force_delete_all = false);

 private:
  // Disallow copying and moving.
  FutureManager(const FutureManager& rhs);
  FutureManager& operator=(const FutureManager& rhs);
  FutureManager(FutureManager&& rhs);
  FutureManager& operator=(FutureManager&& rhs);

  // Insert a ReferenceCountedFutureImpl into the API list, owned by the given
  // object. If any previous ReferenceCountedFutureImpl was owned by the object,
  // it's moved to the orphaned list.
  void InsertFutureApi(void* owner, ReferenceCountedFutureImpl* api);

  // Check whether a ReferenceCountedFutureImpl is safe to delete. It's safe to
  // delete if:
  // - No Futures are pending.
  // - No external references to Futures. (This means that futures in the
  //   LastResult list have one reference, and any others have zero references.)
  bool IsSafeToDeleteFutureApi(ReferenceCountedFutureImpl* api);

  // For handling Future APIs.
  Mutex future_api_mutex_;
  std::map<void*, ReferenceCountedFutureImpl*> future_apis_;
  std::set<ReferenceCountedFutureImpl*> orphaned_future_apis_;
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_FUTURE_MANAGER_H_
