#include "app/src/cleanup_notifier.h"

#include <assert.h>
#include <algorithm>
#include <mutex>
#include <thread>
#include <map>
#include <vector>
#include <new>

namespace firebase {

namespace {

template <typename T>
class NoDestructor {
 public:
  NoDestructor() { new (storage_) T(); }
  T& get() { return *reinterpret_cast<T*>(storage_); }
 private:
  alignas(T) char storage_[sizeof(T)];
};

std::mutex& GetOwnersMutex() {
  static NoDestructor<std::mutex> m;
  return m.get();
}

std::map<void *, CleanupNotifier *>& GetOwnersMap() {
  static NoDestructor<std::map<void *, CleanupNotifier *>> m;
  return m.get();
}
}  // namespace

CleanupNotifier::CleanupNotifier() : cleaned_up_(false) {}

CleanupNotifier::~CleanupNotifier() {
  CleanupAll();
  UnregisterAllOwners();
}

void CleanupNotifier::RegisterObject(void *object, CleanupCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_[object] = std::move(callback);
}

void CleanupNotifier::UnregisterObject(void *object) {
  std::unique_lock<std::mutex> lock(mutex_);
  callbacks_.erase(object);
  cv_.wait(lock, [&]() {
    return in_flight_object_ != object || in_flight_thread_ == std::this_thread::get_id();
  });
}

void CleanupNotifier::CleanupAll() {
  while (true) {
    void* obj = nullptr;
    CleanupCallback cb = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (cleaned_up_ && callbacks_.empty()) {
        break;
      }
      if (callbacks_.empty()) {
        cleaned_up_ = true;
        break;
      }
      auto it = callbacks_.begin();
      obj = it->first;
      cb = std::move(it->second);
      callbacks_.erase(it);
      in_flight_object_ = obj;
      in_flight_thread_ = std::this_thread::get_id();
    }
    
    if (cb) {
      cb(obj);
    }
    
    {
      std::lock_guard<std::mutex> lock(mutex_);
      in_flight_object_ = nullptr;
      in_flight_thread_ = std::thread::id();
      cv_.notify_all();
    }
  }
}

void CleanupNotifier::UnregisterAllOwners() {
  std::lock_guard<std::mutex> lock(GetOwnersMutex());
  while (!owners_.empty()) {
    UnregisterOwner(GetOwnersMap().find(owners_[0]));
  }
}

void CleanupNotifier::RegisterOwner(CleanupNotifier *notifier, void *owner) {
  std::lock_guard<std::mutex> lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  if (it != map.end()) UnregisterOwner(it);
  map[owner] = notifier;
  notifier->owners_.push_back(owner);
}

void CleanupNotifier::UnregisterOwner(CleanupNotifier *notifier, void *owner) {
  std::lock_guard<std::mutex> lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  if (it != map.end()) UnregisterOwner(it);
}

void CleanupNotifier::UnregisterOwner(
    std::map<void *, CleanupNotifier *>::iterator it) {
  auto& map = GetOwnersMap();
  if (it == map.end()) return;
  void *owner = it->first;
  CleanupNotifier *notifier = it->second;
  map.erase(it);
  auto *owners = &notifier->owners_;
  auto owner_it = std::find(owners->begin(), owners->end(), owner);
  if (owner_it != owners->end()) {
    owners->erase(owner_it);
  }
}

CleanupNotifier *CleanupNotifier::FindByOwner(void *owner) {
  std::lock_guard<std::mutex> lock(GetOwnersMutex());
  auto& map = GetOwnersMap();
  auto it = map.find(owner);
  return it != map.end() ? it->second : nullptr;
}

}  // namespace firebase
