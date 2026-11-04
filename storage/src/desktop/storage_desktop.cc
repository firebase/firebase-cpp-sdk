// Copyright 2017 Google LLC

#include "storage/src/desktop/storage_desktop.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/log.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/desktop/storage_reference_desktop.h"

namespace firebase::storage::internal {

StorageInternal::StorageInternal(App* app, const char* url) : app_(app) {
  if (!app_) return;
  {
    std::lock_guard<std::mutex> lock(lifetime_token_->mutex);
    lifetime_token_->storage = this;
  }
  if (url && url[0] != '\0') {
    url_ = url;
  } else {
    const char* bucket = app_->options().storage_bucket();
    if (bucket && bucket[0] != '\0') url_ = std::string(kGsScheme) + bucket;
  }
  root_ = StoragePath(this, url_);

  firebase::rest::util::Initialize();
  firebase::rest::InitTransportCurl();
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthStartTokenListener, app_, nullptr, nullptr);

  std::string sdk, version;
  app_common::GetOuterMostSdkAndVersion(&sdk, &version);
  assert(!sdk.empty() && !version.empty());
  constexpr size_t kPrefixLen = sizeof(FIREBASE_USER_AGENT_PREFIX) - 1;
  if (sdk.compare(0, kPrefixLen, FIREBASE_USER_AGENT_PREFIX) == 0) {
    sdk = sdk.substr(kPrefixLen);
  }
  user_agent_ = sdk + "/" + version;
}

StorageInternal::~StorageInternal() {
  if (!app_) {
    std::lock_guard<std::mutex> lock(lifetime_token_->mutex);
    lifetime_token_->shutting_down = true;
    lifetime_token_->storage = nullptr;
    return;
  }
  {
    std::lock_guard<std::mutex> lock(lifetime_token_->mutex);
    lifetime_token_->shutting_down = true;
    lifetime_token_->cv.notify_all();
  }
  listener_dispatcher_.Shutdown();

  {
    std::vector<std::shared_ptr<RestOperation::OperationHandle>> handles;
    {
      std::lock_guard<std::mutex> lock(operations_mutex_);
      handles.reserve(operations_.size());
      for (const auto& tracked : operations_) {
        if (tracked.operation) {
          handles.push_back(tracked.operation->operation_handle());
        }
      }
    }
    for (const auto& handle : handles) {
      if (handle) {
        std::lock_guard<std::recursive_mutex> op_lock(handle->mutex);
        if (handle->op) handle->op->Cancel();
      }
    }
  }

  {
    std::unique_lock<std::mutex> lock(lifetime_token_->mutex);
    const auto self_id = std::this_thread::get_id();
    const auto only_self = [self_id](const std::vector<std::thread::id>& ids) {
      return std::all_of(ids.begin(), ids.end(),
                         [self_id](const auto& id) { return id == self_id; });
    };
    lifetime_token_->cv.wait(lock, [this, &only_self]() {
      return only_self(lifetime_token_->retry_threads) &&
             only_self(lifetime_token_->active_users);
    });
    lifetime_token_->storage = nullptr;
  }

  CleanupCompletedOperations();
  cleanup_.CleanupAll();
  CleanupAllOperations();
  firebase::rest::CleanupTransportCurl();
  firebase::rest::util::Terminate();
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthStopTokenListener, app_, nullptr, nullptr);
  app_ = nullptr;
}

StorageReferenceInternal* StorageInternal::GetReference() {
  if (!app_) return nullptr;
  MutexLock lock(host_mutex_);
  configured_ = true;
  return new StorageReferenceInternal(root_, this);
}

StorageReferenceInternal* StorageInternal::GetReference(const char* path) {
  if (!app_ || !path) return nullptr;
  MutexLock lock(host_mutex_);
  configured_ = true;
  return new StorageReferenceInternal(root_.GetChild(path), this);
}

StorageReferenceInternal* StorageInternal::GetReferenceFromUrl(
    const char* url) {
  if (!app_ || !url) return nullptr;
  StoragePath parsed_path(this, url);
  if (!parsed_path.IsValid()) {
    LogError("Invalid Storage URL: '%s'", url);
    return nullptr;
  }
  MutexLock lock(host_mutex_);
  configured_ = true;
  return new StorageReferenceInternal(parsed_path, this);
}

std::string StorageInternal::GetAuthToken() {
  if (!app_) return "";
  std::string result;
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetCurrentToken, app(), nullptr, &result);
  return result;
}

uint64_t StorageInternal::AddOperation(RestOperation* operation) {
  if (!operation) return 0;
  CleanupCompletedOperations();
  std::lock_guard<std::mutex> lock(operations_mutex_);
  auto it = std::find_if(operations_.begin(), operations_.end(),
                         [operation](const TrackedOperation& t) {
                           return t.operation == operation;
                         });
  if (it != operations_.end()) return it->id;
  const uint64_t id = next_operation_id_++;
  operations_.push_back(TrackedOperation{operation, id});
  return id;
}

void StorageInternal::RemoveOperation(RestOperation* operation) {
  std::lock_guard<std::mutex> lock(operations_mutex_);
  auto it = std::find_if(operations_.begin(), operations_.end(),
                         [operation](const TrackedOperation& t) {
                           return t.operation == operation;
                         });
  if (it != operations_.end()) operations_.erase(it);
}

void StorageInternal::DeleteOperation(RestOperation* operation,
                                      uint64_t operation_id) {
  if (!operation || operation_id == 0) return;
  {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    auto it = std::find_if(
        operations_.begin(), operations_.end(),
        [operation_id](const TrackedOperation& t) { return t.id == operation_id; });
    if (it == operations_.end()) return;
    operation = it->operation;
    operations_.erase(it);
    deleting_operations_.insert(operation_id);
  }
  delete operation;
  std::lock_guard<std::mutex> lock(operations_mutex_);
  deleting_operations_.erase(operation_id);
  operations_cv_.notify_all();
}

void StorageInternal::CleanupCompletedOperations() {
  std::vector<TrackedOperation> to_delete;
  {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    for (auto it = operations_.begin(); it != operations_.end();) {
      if (it->operation->is_complete()) {
        to_delete.push_back(*it);
        deleting_operations_.insert(it->id);
        it = operations_.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (const auto& tracked : to_delete) {
    delete tracked.operation;
    std::lock_guard<std::mutex> lock(operations_mutex_);
    deleting_operations_.erase(tracked.id);
    operations_cv_.notify_all();
  }
}

void StorageInternal::CleanupAllOperations() {
  while (true) {
    std::vector<TrackedOperation> to_delete;
    {
      std::lock_guard<std::mutex> lock(operations_mutex_);
      for (const auto& tracked : operations_) {
        to_delete.push_back(tracked);
        deleting_operations_.insert(tracked.id);
      }
      operations_.clear();
    }
    for (const auto& tracked : to_delete) {
      tracked.operation->Cancel();
      delete tracked.operation;
      std::lock_guard<std::mutex> lock(operations_mutex_);
      deleting_operations_.erase(tracked.id);
      operations_cv_.notify_all();
    }
    std::unique_lock<std::mutex> lock(operations_mutex_);
    if (operations_.empty() && deleting_operations_.empty()) break;
    if (operations_.empty()) {
      operations_cv_.wait(lock, [this]() {
        return deleting_operations_.empty() || !operations_.empty();
      });
    }
  }
}

void StorageInternal::UseEmulator(const char* host, int port) {
  if (!host || host[0] == '\0') {
    LogError("Emulator host cannot be null or empty.");
    return;
  }
  std::string host_str(host);
  const auto colon_count =
      static_cast<size_t>(std::count(host_str.begin(), host_str.end(), ':'));
  const bool is_bracketed_ipv6 =
      host_str.size() >= 2 && host_str.front() == '[' &&
      host_str.back() == ']' && host_str.find(']') == host_str.size() - 1 &&
      colon_count >= 1;
  const bool is_unbracketed_ipv6 =
      host_str.find('[') == std::string::npos &&
      host_str.find(']') == std::string::npos && colon_count >= 2;
  if (host_str.find('/') != std::string::npos ||
      host_str.find(' ') != std::string::npos ||
      (colon_count > 0 && !is_bracketed_ipv6 && !is_unbracketed_ipv6)) {
    LogError("Emulator host '%s' must be a bare hostname or IP address.", host);
    return;
  }
  if (port <= 0 || port > 65535) {
    LogError("Emulator port must be between 1 and 65535.");
    return;
  }
  MutexLock lock(host_mutex_);
  if (configured_) {
    LogError(
        "Cannot connect to emulator after Storage SDK initialization. "
        "Call UseEmulator(host, port) before creating a Storage "
        "reference or trying to load data.");
    return;
  }
  scheme_ = "http";
  port_ = port;
  host_ = is_unbracketed_ipv6 ? ("[" + host_str + "]") : host_str;
}

StorageLifetimeScope::StorageLifetimeScope(
    std::shared_ptr<StorageInternal::LifetimeToken> token, Mode mode) {
  if (!token) return;
  std::lock_guard<std::mutex> lock(token->mutex);
  if (!token->storage ||
      (mode == Mode::kRejectShutdown && token->shutting_down)) {
    return;
  }
  id_ = std::this_thread::get_id();
  token->active_users.push_back(id_);
  storage_ = token->storage;
  token_ = std::move(token);
}

StorageLifetimeScope::~StorageLifetimeScope() {
  if (!token_) return;
  std::lock_guard<std::mutex> lock(token_->mutex);
  auto it =
      std::find(token_->active_users.begin(), token_->active_users.end(), id_);
  if (it != token_->active_users.end()) token_->active_users.erase(it);
  token_->cv.notify_all();
}

}  // namespace firebase::storage::internal
