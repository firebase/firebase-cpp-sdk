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
#include <map>
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

// Keeps the FutureBackingData and ReferenceCountedFutureImpl alive for the full
// duration of an asynchronous Repo operation (including server ACK latency),
// and atomically completes the user handle either when Repo resolves the
// internal proxy future or with an error if the Repo shuts down first.
template <typename T>
struct AsyncOpState : public std::enable_shared_from_this<AsyncOpState<T>> {
  AsyncOpState(std::shared_ptr<ReferenceCountedFutureImpl> api_in,
               SafeFutureHandle<T> handle_in,
               void* context_in = nullptr,
               void (*delete_context_in)(void*) = nullptr)
      : user_api(std::move(api_in)),
        user_handle(handle_in),
        context(context_in),
        delete_context(delete_context_in),
        repo_api(1),
        repo_handle(RepoFutureTraits<T>::Alloc(&repo_api)),
        repo_future(MakeFuture(&repo_api, repo_handle)),
        completed(false),
        ready_to_release(false) {}

  ~AsyncOpState() { FailIfNotCompleted(); }

  static std::shared_ptr<AsyncOpState<T>> Create(
      std::shared_ptr<ReferenceCountedFutureImpl> api_in,
      SafeFutureHandle<T> handle_in,
      void* context_in = nullptr,
      void (*delete_context_in)(void*) = nullptr) {
    auto state = std::make_shared<AsyncOpState<T>>(
        std::move(api_in), handle_in, context_in, delete_context_in);
    std::weak_ptr<AsyncOpState<T>> weak_self = state;
    state->repo_future.OnCompletion([weak_self](const Future<T>& result) {
      if (auto self = weak_self.lock()) {
        self->CompleteFromRepo(result);
      }
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
    if (delete_fn != nullptr) {
      delete_fn(context_to_delete);
    }
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
  bool completed;
  bool ready_to_release;
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

}  // namespace

const char kVirtualChildKeyValue[] = ".value";
const char kVirtualChildKeyPriority[] = ".priority";

DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseInternal* database,
                                                     const Path& path)
    : QueryInternal(database, QuerySpec(path)),
      ref_future_impl_(std::make_shared<ReferenceCountedFutureImpl>(
          kDatabaseReferenceFnCount)) {}

DatabaseReferenceInternal::~DatabaseReferenceInternal() = default;

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& internal)
    : QueryInternal(internal),
      ref_future_impl_(std::make_shared<ReferenceCountedFutureImpl>(
          kDatabaseReferenceFnCount)) {}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& internal) {
  if (&internal == this) return *this;
  QueryInternal::operator=(internal);
  ref_future_impl_ =
      std::make_shared<ReferenceCountedFutureImpl>(kDatabaseReferenceFnCount);
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
  if (&internal == this) return *this;
  QueryInternal::operator=(std::move(internal));
  ref_future_impl_ = std::move(internal.ref_future_impl_);
  internal.database_ = nullptr;
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
  if (!query_spec_.path.str().empty()) {
    url << '/';
    url << query_spec_.path.str();
  }
  return url.str();
}

bool DatabaseReferenceInternal::IsRoot() const {
  return query_spec_.path.empty();
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() {
  if (!database_) return nullptr;
  return new DatabaseReferenceInternal(database_, query_spec_.path.GetParent());
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetRoot() {
  if (!database_) return nullptr;
  return new DatabaseReferenceInternal(database_, Path());
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(const char* path) {
  if (!database_ || path == nullptr) return nullptr;
  return new DatabaseReferenceInternal(database_,
                                       query_spec_.path.GetChild(path));
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() {
  if (!database_) return nullptr;
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  std::string child = database_->name_generator().GeneratePushChildName(now);
  return new DatabaseReferenceInternal(database_,
                                       query_spec_.path.GetChild(child));
}

void DatabaseReferenceInternal::RemoveValueImpl(SafeFutureHandle<void> handle) {
  if (!database_ || !ref_future_impl_) {
    if (ref_future_impl_) {
      ref_future_impl_->Complete(handle, kErrorUnknownError,
                                 "The database is being destroyed.");
    }
    return;
  }
  auto state = AsyncOpState<void>::Create(ref_future_impl_, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->SetValue(path, Variant::Null(),
                                        &op_state->repo_api,
                                        op_state->repo_handle);
          ScheduleOpStateWatch(ref, std::move(op_state));
        } else {
          op_state->FailIfNotCompleted();
        }
      },
      database_->repo()->this_ref(), query_spec_.path, std::move(state)));
}


Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    void (*delete_context)(void*), bool trigger_local_events) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) {
    if (delete_context != nullptr) {
      delete_context(context);
    }
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


void DatabaseReferenceInternal::SetPriorityImpl(const Variant& priority, SafeFutureHandle<void> handle) {
  if (!database_ || !ref_future_impl_) {
    if (ref_future_impl_) {
      ref_future_impl_->Complete(handle, kErrorUnknownError,
                                 "The database is being destroyed.");
    }
    return;
  }
  auto state = AsyncOpState<void>::Create(ref_future_impl_, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant prio,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          ConvertVectorToMap(&prio);
          lock.GetReference()->SetValue(path.GetChild(kPriorityKey), prio,
                                        &op_state->repo_api,
                                        op_state->repo_handle);
          ScheduleOpStateWatch(ref, std::move(op_state));
        } else {
          op_state->FailIfNotCompleted();
        }
      },
      database_->repo()->this_ref(), query_spec_.path, priority,
      std::move(state)));
}


void DatabaseReferenceInternal::SetValueImpl(const Variant& value, SafeFutureHandle<void> handle) {
  if (!database_ || !ref_future_impl_) {
    if (ref_future_impl_) {
      ref_future_impl_->Complete(handle, kErrorUnknownError,
                                 "The database is being destroyed.");
    }
    return;
  }
  auto state = AsyncOpState<void>::Create(ref_future_impl_, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant val,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          ConvertVectorToMap(&val);
          lock.GetReference()->SetValue(path, val, &op_state->repo_api,
                                        op_state->repo_handle);
          ScheduleOpStateWatch(ref, std::move(op_state));
        } else {
          op_state->FailIfNotCompleted();
        }
      },
      database_->repo()->this_ref(), query_spec_.path, value,
      std::move(state)));
}


void DatabaseReferenceInternal::SetValueAndPriorityImpl(
    const Variant& value, const Variant& priority, SafeFutureHandle<void> handle) {
  if (!database_ || !ref_future_impl_) {
    if (ref_future_impl_) {
      ref_future_impl_->Complete(handle, kErrorUnknownError,
                                 "The database is being destroyed.");
    }
    return;
  }
  auto state = AsyncOpState<void>::Create(ref_future_impl_, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant val, Variant prio,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          ConvertVectorToMap(&val);
          Variant data;
          if (val.is_map()) {
            data = std::move(val);
            data.map()[kVirtualChildKeyPriority] = std::move(prio);
          } else {
            data = Variant::EmptyMap();
            data.map()[kVirtualChildKeyValue] = std::move(val);
            data.map()[kVirtualChildKeyPriority] = std::move(prio);
          }
          lock.GetReference()->SetValue(path, data, &op_state->repo_api,
                                        op_state->repo_handle);
          ScheduleOpStateWatch(ref, std::move(op_state));
        } else {
          op_state->FailIfNotCompleted();
        }
      },
      database_->repo()->this_ref(), query_spec_.path, value, priority,
      std::move(state)));
}


void DatabaseReferenceInternal::UpdateChildrenImpl(const Variant& values, SafeFutureHandle<void> handle) {
  if (!database_ || !ref_future_impl_) {
    if (ref_future_impl_) {
      ref_future_impl_->Complete(handle, kErrorUnknownError,
                                 "The database is being destroyed.");
    }
    return;
  }
  auto state = AsyncOpState<void>::Create(ref_future_impl_, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant vals,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          ConvertVectorToMap(&vals);
          lock.GetReference()->UpdateChildren(
              path, vals, &op_state->repo_api, op_state->repo_handle);
          ScheduleOpStateWatch(ref, std::move(op_state));
        } else {
          op_state->FailIfNotCompleted();
        }
      },
      database_->repo()->this_ref(), query_spec_.path, values,
      std::move(state)));
}


DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  if (!database_) return nullptr;
  return new DisconnectionHandler(
      new DisconnectionHandlerInternal(database_, query_spec_.path));
}

void DatabaseReferenceInternal::GoOffline() {
  if (database_) database_->GoOffline();
}

void DatabaseReferenceInternal::GoOnline() {
  if (database_) database_->GoOnline();
}

ReferenceCountedFutureImpl* DatabaseReferenceInternal::ref_future() {
  return database_ ? ref_future_impl_.get() : nullptr;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
