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

// Keeps ReferenceCountedFutureImpl alive for the full duration of an
// asynchronous Repo OnDisconnect request (including server ACK latency), and
// atomically completes the user handle either when Repo resolves the internal
// proxy future or with an error if the Database/Repo shuts down first.
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
        repo_future(MakeFuture(repo_api.get(), repo_handle)),
        completed(false),
        cleanup_finished(false) {}

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
        // Defer releasing the CleanupNotifier registration (and thus `self`
        // and `repo_api`) onto Repo::scheduler() BEFORE running user callbacks
        // so `repo_api` is never destroyed while `repo_api->Complete()` is on
        // the call stack.
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
      DatabaseInternal* db = db_lock.GetReference();
      if (db != nullptr) {
        // Keep `this` (and `repo_api`) alive until after `repo_.reset(nullptr)`
        // finishes in `~DatabaseInternal()` so in-flight OnDisconnectResponse
        // objects in Repo/PersistentConnection never hold a dangling pointer.
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
    DatabaseInternal* db = db_lock.GetReference();
    if (db != nullptr) {
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
  bool completed;
  bool cleanup_finished;
};

}  // namespace

DisconnectionHandlerInternal::DisconnectionHandlerInternal(
    DatabaseInternal* database, const Path& path)
    : database_(database),
      path_(path),
      future_impl_(std::make_shared<ReferenceCountedFutureImpl>(
          kDisconnectionHandlerFnCount)) {}

DisconnectionHandlerInternal::~DisconnectionHandlerInternal() = default;

void DisconnectionHandlerInternal::CancelImpl(SafeFutureHandle<void> handle) {
  if (!database_ || !database_->repo() || !future_impl_) {
    if (future_impl_) {
      future_impl_->Complete(handle, kErrorUnknownError,
                             "The database is being destroyed.");
    }
    return;
  }
  auto state = DisconnectionOpState::Create(database_, future_impl_, handle);
  database_->repo()->OnDisconnectCancel(state->repo_handle,
                                        state->repo_api.get(), path_);
}

void DisconnectionHandlerInternal::RemoveValueImpl(
    SafeFutureHandle<void> handle) {
  if (!database_ || !database_->repo() || !future_impl_) {
    if (future_impl_) {
      future_impl_->Complete(handle, kErrorUnknownError,
                             "The database is being destroyed.");
    }
    return;
  }
  auto state = DisconnectionOpState::Create(database_, future_impl_, handle);
  database_->repo()->OnDisconnectSetValue(
      state->repo_handle, state->repo_api.get(), path_, Variant::Null());
}

void DisconnectionHandlerInternal::SetValueImpl(
    Variant value, SafeFutureHandle<void> handle) {
  if (!database_ || !database_->repo() || !future_impl_) {
    if (future_impl_) {
      future_impl_->Complete(handle, kErrorUnknownError,
                             "The database is being destroyed.");
    }
    return;
  }
  auto state = DisconnectionOpState::Create(database_, future_impl_, handle);
  database_->repo()->OnDisconnectSetValue(
      state->repo_handle, state->repo_api.get(), path_, std::move(value));
}

void DisconnectionHandlerInternal::SetValueAndPriorityImpl(
    Variant value, Variant priority, SafeFutureHandle<void> handle) {
  if (!database_ || !database_->repo() || !future_impl_) {
    if (future_impl_) {
      future_impl_->Complete(handle, kErrorUnknownError,
                             "The database is being destroyed.");
    }
    return;
  }
  ConvertVectorToMap(&value);
  Variant data;
  if (value.is_map()) {
    data = std::move(value);
    data.map()[".priority"] = std::move(priority);
  } else {
    data = Variant::EmptyMap();
    data.map()[".value"] = std::move(value);
    data.map()[".priority"] = std::move(priority);
  }
  auto state = DisconnectionOpState::Create(database_, future_impl_, handle);
  database_->repo()->OnDisconnectSetValue(
      state->repo_handle, state->repo_api.get(), path_, std::move(data));
}

void DisconnectionHandlerInternal::UpdateChildrenImpl(
    Variant values, SafeFutureHandle<void> handle) {
  if (!database_ || !database_->repo() || !future_impl_) {
    if (future_impl_) {
      future_impl_->Complete(handle, kErrorUnknownError,
                             "The database is being destroyed.");
    }
    return;
  }
  auto state = DisconnectionOpState::Create(database_, future_impl_, handle);
  database_->repo()->OnDisconnectUpdate(
      state->repo_handle, state->repo_api.get(), path_, std::move(values));
}

ReferenceCountedFutureImpl* DisconnectionHandlerInternal::future() {
  return future_impl_.get();
}

DisconnectionHandler
DisconnectionHandlerInternal::GetInvalidDisconnectionHandler() {
  return DisconnectionHandler(nullptr);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
