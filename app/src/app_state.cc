/*
 * Copyright 2026 Google LLC
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

#include "app/src/app_state.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "app/src/callback.h"
#include "app/src/log.h"

namespace firebase {
namespace internal {

const char kAuthServiceTag = 0;
const char kAppCheckServiceTag = 0;

AppState::AppState(std::string name, AppOptions options)
    : name_(std::move(name)),
      options_(std::move(options)),
      system_logger_(),
      logger_(&system_logger_, kLogLevelVerbose) {
  LogDebug(
      "Created app name=%s: options, api_key=%s, app_id=%s, database_url=%s, "
      "messaging_sender_id=%s, storage_bucket=%s, project_id=%s (0x%08x)",
      name_.c_str(), options_.api_key(), options_.app_id(),
      options_.database_url(), options_.messaging_sender_id(),
      options_.storage_bucket(), options_.project_id(),
      static_cast<int>(reinterpret_cast<intptr_t>(this)));
  callback::Initialize();
}

AppState::~AppState() {
  LogDebug("Deleting app %s (0x%08x)", name_.c_str(),
           static_cast<int>(reinterpret_cast<intptr_t>(this)));
  // Destroy all attached product services first while AppState (options,
  // logger, platform data, JNI/FIRApp) is still alive.
  DestroyServices();
  // Tear down platform-specific state (HeartbeatController, JNI refs, FIRApp).
  DestroyPlatformData();
  callback::Terminate(/*flush_all=*/false);
}

void AppState::DestroyServices() {
  while (true) {
    std::shared_ptr<Service> to_destroy;
    {
      MutexLock lock(services_mutex_);
      if (owned_services_.empty()) {
        index_.clear();
        return;
      }

      // Pick the service with the highest teardown_priority(); break ties by
      // reverse creation order (later entries in owned_services_ first).
      size_t best_idx = owned_services_.size() - 1;
      int best_priority = owned_services_[best_idx].service->teardown_priority();
      for (size_t i = owned_services_.size() - 1; i > 0; --i) {
        size_t candidate = i - 1;
        int priority = owned_services_[candidate].service->teardown_priority();
        if (priority > best_priority) {
          best_priority = priority;
          best_idx = candidate;
        }
      }

      ServiceEntry entry = std::move(owned_services_[best_idx]);
      owned_services_.erase(owned_services_.begin() +
                            static_cast<std::ptrdiff_t>(best_idx));

      ServiceKey key{entry.type_tag, entry.instance_key};
      auto it = index_.find(key);
      if (it != index_.end() && it->second == entry.service.get()) {
        index_.erase(it);
      }
      to_destroy = std::move(entry.service);
    }
    // Destroy outside services_mutex_ so the service's destructor can safely
    // call FindService() or join worker threads without deadlocking.
    to_destroy.reset();
  }
}

bool AppState::DetachService(const void* type_tag, std::string_view key,
                             const Service* expected) {
  std::shared_ptr<Service> removed;
  {
    MutexLock lock(services_mutex_);
    ServiceKey lookup_key{type_tag, std::string(key)};
    auto it = index_.find(lookup_key);
    if (it == index_.end()) return false;
    if (expected != nullptr && it->second != expected) return false;

    Service* target = it->second;
    index_.erase(it);

    for (auto vec_it = owned_services_.begin(); vec_it != owned_services_.end();
         ++vec_it) {
      if (vec_it->service.get() == target) {
        removed = std::move(vec_it->service);
        owned_services_.erase(vec_it);
        break;
      }
    }
  }
  // Drop `removed` outside services_mutex_.
  return true;
}

}  // namespace internal
}  // namespace firebase
