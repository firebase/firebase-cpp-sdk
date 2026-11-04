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

#include "database/src/desktop/database_reference_desktop.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#include "app/src/variant_util.h"
#include "database/src/common/database_reference.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/disconnection_desktop.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {

using callback::NewCallback;

namespace database {
namespace internal {

namespace {

template <typename T>
struct RepoFutureTraits;

template <>
struct RepoFutureTraits<void> {
  static SafeFutureHandle<void> Alloc(ReferenceCountedFutureImpl* api) {
    return api->SafeAlloc<void>(0);
  }
  static void CompleteFromFuture(ReferenceCountedFutureImpl* api,
                                 SafeFutureHandle<void> handle,
                                 const Future<void>& result) {
    api->Complete(handle, result.error(),
                  result.error_message() ? result.error_message() : "");
  }
};

template <>
struct RepoFutureTraits<DataSnapshot> {
  static SafeFutureHandle<DataSnapshot> Alloc(ReferenceCountedFutureImpl* api) {
    return api->SafeAlloc<DataSnapshot>(0, DataSnapshot(nullptr));
  }
  static void CompleteFromFuture(ReferenceCountedFutureImpl* api,
                                 SafeFutureHandle<DataSnapshot> handle,
                                 const Future<DataSnapshot>& result) {
    const char* msg = result.error_message() ? result.error_message() : "";
    if (result.result() != nullptr) {
      api->CompleteWithResult(handle, result.error(), msg, *result.result());
    } else {
      api->Complete(handle, result.error(), msg);
    }
  }
};

template <typename T>
struct AsyncOpState : public std::enable_shared_from_this<AsyncOpState<T>> {
  AsyncOpState(std::shared_ptr<ReferenceCountedFutureImpl> api_in,
               SafeFutureHandle<T> handle_in, void* context_in = nullptr,
               void (*delete_context_in)(void*) = nullptr)
      : user_api(std::move(api_in)),
        user_handle(handle_in),
        context(context_in),
        delete_context(delete_context_in),
        repo_api(1),
        repo_handle(RepoFutureTraits<T>::Alloc(&repo_api)),
        repo_future(MakeFuture(&repo_api, repo_handle)) {}

  ~AsyncOpState() { FailIfNotCompleted(); }

  static std::shared_ptr<AsyncOpState<T>> Create(
      std::shared_ptr<ReferenceCountedFutureImpl> api_in,
      SafeFutureHandle<T> handle_in, void* context_in = nullptr,
      void (*delete_context_in)(void*) = nullptr) {
    auto state = std::make_shared<AsyncOpState<T>>(
        std::move(api_in), handle_in, context_in, delete_context_in);
    std::weak_ptr<AsyncOpState<T>> weak_self = state;
    state->repo_future.OnCompletion([weak_self](const Future<T>& result) {
      if (auto self = weak_self.lock()) self->CompleteFromRepo(result);
    });
    return state;
  }

  void ReleaseContextOwnership() {
    std::lock_guard<std::mutex> lock(mutex);
    delete_context = nullptr;
  }

  void CompleteFromRepo(const Future<T>& result) {
    std::shared_ptr<ReferenceCountedFutureImpl> api_to_complete;
    SafeFutureHandle<T> handle_to_complete;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (completed) return;
      completed = true;
      api_to_complete = std::move(user_api);
      handle_to_complete = user_handle;
    }
    if (api_to_complete != nullptr) {
      RepoFutureTraits<T>::CompleteFromFuture(api_to_complete.get(),
                                              handle_to_complete, result);
    }
  }

  void FailIfNotCompleted() {
    std::shared_ptr<ReferenceCountedFutureImpl> api_to_complete;
    SafeFutureHandle<T> handle_to_complete;
    void* context_to_delete = nullptr;
    void (*delete_fn)(void*) = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (delete_context != nullptr) {
        context_to_delete = context;
        delete_fn = delete_context;
        delete_context = nullptr;
      }
      if (!completed) {
        completed = true;
        api_to_complete = std::move(user_api);
        handle_to_complete = user_handle;
      }
    }
    if (delete_fn != nullptr) delete_fn(context_to_delete);
    if (api_to_complete != nullptr) {
      api_to_complete->Complete(handle_to_complete, kErrorUnknownError,
                                "The database is being destroyed.");
    }
  }

  bool ShouldStopWatching() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!completed) return false;
    if (!ready_to_release) {
      ready_to_release = true;
      return false;
    }
    return true;
  }

  std::mutex mutex;
  std::shared_ptr<ReferenceCountedFutureImpl> user_api;
  SafeFutureHandle<T> user_handle;
  void* context;
  void (*delete_context)(void*);
  ReferenceCountedFutureImpl repo_api;
  SafeFutureHandle<T> repo_handle;
  Future<T> repo_future;
  bool completed = false;
  bool ready_to_release = false;
};

template <typename T>
void ScheduleOpStateWatch(Repo::ThisRef ref,
                          std::shared_ptr<AsyncOpState<T>> op_state) {
  Repo::scheduler().Schedule(
      NewCallback(
          [](Repo::ThisRef ref, std::shared_ptr<AsyncOpState<T>> op_state) {
            if (op_state->ShouldStopWatching()) return;
            Repo::ThisRefLock lock(&ref);
            if (lock.GetReference() == nullptr) {
              op_state->FailIfNotCompleted();
              return;
            }
            ScheduleOpStateWatch(ref, std::move(op_state));
          },
          ref, std::move(op_state)),
      5);
}

void ScheduleVoidRepoOp(
    DatabaseInternal* database,
    const std::shared_ptr<ReferenceCountedFutureImpl>& future_impl,
    SafeFutureHandle<void> handle, const Path& path, const Variant& val,
    bool is_update = false) {
  if (!database || !future_impl) {
    if (future_impl) {
      future_impl->Complete(handle, kErrorUnknownError,
                            "The database is being destroyed.");
    }
    return;
  }
  auto state = AsyncOpState<void>::Create(future_impl, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant val, bool is_update,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          ConvertVectorToMap(&val);
          if (is_update) {
            lock.GetReference()->UpdateChildren(path, val, &op_state->repo_api,
                                                op_state->repo_handle);
          } else {
            lock.GetReference()->SetValue(path, val, &op_state->repo_api,
                                          op_state->repo_handle);
          }
          ScheduleOpStateWatch(ref, std::move(op_state));
        } else {
          op_state->FailIfNotCompleted();
        }
      },
      database->repo()->this_ref(), path, val, is_update, std::move(state)));
}

}  // namespace

const char kVirtualChildKeyValue[] = ".value";
const char kVirtualChildKeyPriority[] = ".priority";

DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseInternal* database,
                                                     const Path& path)
    : QueryInternal(database, QuerySpec(path)),
      ref_future_impl_(std::make_shared<ReferenceCountedFutureImpl>(
          kDatabaseReferenceFnCount)) {}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& internal)
    : QueryInternal(internal),
      ref_future_impl_(std::make_shared<ReferenceCountedFutureImpl>(
          kDatabaseReferenceFnCount)) {}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& internal) {
  if (&internal != this) {
    QueryInternal::operator=(internal);
    ref_future_impl_ =
        std::make_shared<ReferenceCountedFutureImpl>(kDatabaseReferenceFnCount);
  }
  return *this;
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& internal) noexcept
    : QueryInternal(std::move(internal)),
      ref_future_impl_(std::move(internal.ref_future_impl_)) {
  internal.database_ = nullptr;
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    DatabaseReferenceInternal&& internal) noexcept {
  if (&internal != this) {
    QueryInternal::operator=(std::move(internal));
    ref_future_impl_ = std::move(internal.ref_future_impl_);
    internal.database_ = nullptr;
  }
  return *this;
}

Database* DatabaseReferenceInternal::GetDatabase() const {
  if (!database_) return nullptr;
  return database_->constructor_url().empty()
             ? Database::GetInstance(database_->GetApp())
             : Database::GetInstance(database_->GetApp(),
                                     database_->constructor_url().c_str());
}

std::string DatabaseReferenceInternal::GetUrl() const {
  if (!database_) return std::string();
  std::stringstream url;
  url << database_->database_url();
  if (!query_spec_.path.str().empty()) url << '/' << query_spec_.path.str();
  return url.str();
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() {
  return database_
             ? new DatabaseReferenceInternal(database_,
                                             query_spec_.path.GetParent())
             : nullptr;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetRoot() {
  return database_ ? new DatabaseReferenceInternal(database_, Path()) : nullptr;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(const char* path) {
  return (database_ && path)
             ? new DatabaseReferenceInternal(database_,
                                             query_spec_.path.GetChild(path))
             : nullptr;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() {
  if (!database_) return nullptr;
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  return new DatabaseReferenceInternal(
      database_,
      query_spec_.path.GetChild(
          database_->name_generator().GeneratePushChildName(now)));
}

void DatabaseReferenceInternal::RemoveValueImpl(SafeFutureHandle<void> handle) {
  ScheduleVoidRepoOp(database_, ref_future_impl_, handle, query_spec_.path,
                     Variant::Null());
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    void (*delete_context)(void*), bool trigger_local_events) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) {
    if (delete_context != nullptr) delete_context(context);
    return Future<DataSnapshot>();
  }
  SafeFutureHandle<DataSnapshot> handle = api->SafeAlloc<DataSnapshot>(
      kDatabaseReferenceFnRunTransaction, DataSnapshot(nullptr));
  Future<DataSnapshot> result = MakeFuture(api, handle);
  auto state = AsyncOpState<DataSnapshot>::Create(
      ref_future_impl_, handle, context, delete_context);

  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path,
         DoTransactionWithContext transaction_function,
         bool trigger_local_events,
         std::shared_ptr<AsyncOpState<DataSnapshot>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          void* ctx = op_state->context;
          void (*del_ctx)(void*) = op_state->delete_context;
          op_state->ReleaseContextOwnership();
          lock.GetReference()->StartTransaction(
              path, transaction_function, ctx, del_ctx, trigger_local_events,
              &op_state->repo_api, op_state->repo_handle);
          ScheduleOpStateWatch(ref, std::move(op_state));
        } else {
          op_state->FailIfNotCompleted();
        }
      },
      database_->repo()->this_ref(), query_spec_.path, transaction_function,
      trigger_local_events, std::move(state)));
  return result;
}

void DatabaseReferenceInternal::SetPriorityImpl(const Variant& priority,
                                                SafeFutureHandle<void> handle) {
  ScheduleVoidRepoOp(database_, ref_future_impl_, handle,
                     query_spec_.path.GetChild(kPriorityKey), priority);
}

void DatabaseReferenceInternal::SetValueImpl(const Variant& value,
                                             SafeFutureHandle<void> handle) {
  ScheduleVoidRepoOp(database_, ref_future_impl_, handle, query_spec_.path,
                     value);
}

void DatabaseReferenceInternal::SetValueAndPriorityImpl(
    const Variant& value, const Variant& priority,
    SafeFutureHandle<void> handle) {
  Variant val = value;
  ConvertVectorToMap(&val);
  Variant data = Variant::EmptyMap();
  if (val.is_map()) {
    data = std::move(val);
  } else {
    data.map()[kVirtualChildKeyValue] = std::move(val);
  }
  data.map()[kVirtualChildKeyPriority] = priority;
  ScheduleVoidRepoOp(database_, ref_future_impl_, handle, query_spec_.path,
                     data);
}

void DatabaseReferenceInternal::UpdateChildrenImpl(
    const Variant& values, SafeFutureHandle<void> handle) {
  ScheduleVoidRepoOp(database_, ref_future_impl_, handle, query_spec_.path,
                     values, /*is_update=*/true);
}

DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  return database_ ? new DisconnectionHandler(new DisconnectionHandlerInternal(
                         database_, query_spec_.path))
                   : nullptr;
}

void DatabaseReferenceInternal::GoOffline() {
  if (database_) database_->GoOffline();
}
void DatabaseReferenceInternal::GoOnline() {
  if (database_) database_->GoOnline();
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
