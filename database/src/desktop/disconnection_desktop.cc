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

#include "database/src/desktop/disconnection_desktop.h"

#include <memory>
#include <mutex>
#include <utility>

#include "app/src/callback.h"
#include "app/src/variant_util.h"
#include "database/src/common/database_reference.h"
#include "database/src/desktop/core/repo.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {

using callback::NewCallback;

namespace database {
namespace internal {

namespace {

struct DisconnectionOpState
    : public std::enable_shared_from_this<DisconnectionOpState> {
  DisconnectionOpState(DatabaseInternal* database,
                       std::shared_ptr<ReferenceCountedFutureImpl> api_in,
                       SafeFutureHandle<void> handle_in)
      : db_ref(database->this_ref()),
        repo_ref(database->repo()->this_ref()),
        user_api(std::move(api_in)),
        user_handle(handle_in),
        user_backing_future(MakeFuture(user_api.get(), user_handle)),
        repo_api(std::make_shared<ReferenceCountedFutureImpl>(1)),
        repo_handle(repo_api->SafeAlloc<void>(0)),
        repo_future(MakeFuture(repo_api.get(), repo_handle)) {}

  ~DisconnectionOpState() { FailIfNotCompleted(); }

  static std::shared_ptr<DisconnectionOpState> Create(
      DatabaseInternal* database,
      std::shared_ptr<ReferenceCountedFutureImpl> api_in,
      SafeFutureHandle<void> handle_in) {
    auto state = std::make_shared<DisconnectionOpState>(
        database, std::move(api_in), handle_in);
    database->cleanup().RegisterObject(
        state.get(), [state](void*) { state->OnDatabaseCleanup(); });
    std::weak_ptr<DisconnectionOpState> weak_self = state;
    state->repo_future.OnCompletion([weak_self](const Future<void>& result) {
      if (auto self = weak_self.lock()) {
        {
          Repo::ThisRefLock repo_lock(&self->repo_ref);
          if (repo_lock.GetReference() != nullptr) {
            Repo::scheduler().Schedule(NewCallback(
                [](std::shared_ptr<DisconnectionOpState> op_state) {
                  op_state->UnregisterFromCleanup();
                },
                self));
          }
        }
        self->CompleteOnce(result.error(),
                           result.error_message() ? result.error_message() : "");
      }
    });
    return state;
  }

  void CompleteOnce(int error, const char* error_msg) {
    std::shared_ptr<ReferenceCountedFutureImpl> api_to_complete;
    SafeFutureHandle<void> handle_to_complete;
    Future<void> backing_to_release;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (completed) return;
      completed = true;
      api_to_complete = user_api;
      handle_to_complete = user_handle;
      backing_to_release = std::move(user_backing_future);
    }
    if (api_to_complete != nullptr) {
      api_to_complete->Complete(handle_to_complete, error, error_msg);
    }
    backing_to_release.Release();
  }

  void FailIfNotCompleted() {
    CompleteOnce(kErrorUnknownError, "The database is being destroyed.");
  }

  void OnDatabaseCleanup() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      cleanup_finished = true;
    }
    {
      DatabaseInternal::ThisRefLock db_lock(&db_ref);
      if (DatabaseInternal* db = db_lock.GetReference()) {
        db->RetainUntilRepoDestroyed(shared_from_this());
      }
    }
    FailIfNotCompleted();
  }

  void UnregisterFromCleanup() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (cleanup_finished) return;
      cleanup_finished = true;
    }
    DatabaseInternal::ThisRefLock db_lock(&db_ref);
    if (DatabaseInternal* db = db_lock.GetReference()) {
      db->cleanup().UnregisterObject(this);
    }
  }

  DatabaseInternal::ThisRef db_ref;
  Repo::ThisRef repo_ref;
  std::mutex mutex;
  std::shared_ptr<ReferenceCountedFutureImpl> user_api;
  SafeFutureHandle<void> user_handle;
  Future<void> user_backing_future;
  std::shared_ptr<ReferenceCountedFutureImpl> repo_api;
  SafeFutureHandle<void> repo_handle;
  Future<void> repo_future;
  bool completed = false;
  bool cleanup_finished = false;
};

template <typename Fn>
void ExecuteDisconnectOp(
    DatabaseInternal* db,
    const std::shared_ptr<ReferenceCountedFutureImpl>& future_impl,
    SafeFutureHandle<void> handle, Fn fn) {
  if (!db || !db->repo() || !future_impl) {
    if (future_impl) {
      future_impl->Complete(handle, kErrorUnknownError,
                            "The database is being destroyed.");
    }
    return;
  }
  auto state = DisconnectionOpState::Create(db, future_impl, handle);
  fn(db->repo(), state->repo_handle, state->repo_api.get());
}

}  // namespace

DisconnectionHandlerInternal::DisconnectionHandlerInternal(
    DatabaseInternal* database, const Path& path)
    : database_(database),
      path_(path),
      future_impl_(std::make_shared<ReferenceCountedFutureImpl>(
          kDisconnectionHandlerFnCount)) {}

void DisconnectionHandlerInternal::CancelImpl(SafeFutureHandle<void> handle) {
  ExecuteDisconnectOp(database_, future_impl_, handle,
                      [this](Repo* repo, SafeFutureHandle<void> h,
                             ReferenceCountedFutureImpl* api) {
                        repo->OnDisconnectCancel(h, api, path_);
                      });
}

void DisconnectionHandlerInternal::RemoveValueImpl(
    SafeFutureHandle<void> handle) {
  SetValueImpl(Variant::Null(), handle);
}

void DisconnectionHandlerInternal::SetValueImpl(Variant value,
                                                SafeFutureHandle<void> handle) {
  ExecuteDisconnectOp(database_, future_impl_, handle,
                      [this, value = std::move(value)](
                          Repo* repo, SafeFutureHandle<void> h,
                          ReferenceCountedFutureImpl* api) mutable {
                        repo->OnDisconnectSetValue(h, api, path_,
                                                   std::move(value));
                      });
}

void DisconnectionHandlerInternal::SetValueAndPriorityImpl(
    Variant value, Variant priority, SafeFutureHandle<void> handle) {
  ConvertVectorToMap(&value);
  Variant data = Variant::EmptyMap();
  if (value.is_map()) {
    data = std::move(value);
  } else {
    data.map()[".value"] = std::move(value);
  }
  data.map()[".priority"] = std::move(priority);
  SetValueImpl(std::move(data), handle);
}

void DisconnectionHandlerInternal::UpdateChildrenImpl(
    Variant values, SafeFutureHandle<void> handle) {
  ExecuteDisconnectOp(database_, future_impl_, handle,
                      [this, values = std::move(values)](
                          Repo* repo, SafeFutureHandle<void> h,
                          ReferenceCountedFutureImpl* api) mutable {
                        repo->OnDisconnectUpdate(h, api, path_,
                                                 std::move(values));
                      });
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
