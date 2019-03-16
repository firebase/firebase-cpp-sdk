// Copyright 2016 Google LLC
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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_LISTENER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_LISTENER_H_

#include <algorithm>
#include <map>
#include "app/src/mutex.h"
#include "database/src/common/query_spec.h"

namespace firebase {
namespace database {

class ValueListener;
class ChildListener;

template <typename T>
class ListenerCollection {
 public:
  // Insert a value into a map of <key, vector<value>>.
  // Returns true if it was inserted, false if it was already in the list.
  template <typename K, typename V>
  static bool InsertIntoValueVectorAtKey(std::map<K, std::vector<V>>* map,
                                         const K& key, const V& value) {
    auto i = map->find(key);
    if (i == map->end()) {
      // Add new key with 1-element vector.
      std::vector<V> v;
      v.push_back(value);
      map->insert(std::make_pair(key, v));
      return true;
    } else {
      // Add to existing vector if not already in list.
      auto j = std::find(i->second.begin(), i->second.end(), value);
      if (j == i->second.end()) {
        i->second.push_back(value);
        return true;
      }
      return false;
    }
  }

  // Remove a value into a map of <key, vector<value>>. Remove the key entirely
  // if the vector ends up empty. Returns true if successful.
  template <typename K, typename V>
  static bool RemoveFromValueVectorAtKey(std::map<K, std::vector<V>>* map,
                                         const K& key, const V& value) {
    auto i = map->find(key);
    if (i != map->end()) {
      auto j = std::find(i->second.begin(), i->second.end(), value);
      if (j != i->second.end()) {
        i->second.erase(j);
        if (i->second.size() == 0) {
          map->erase(i);
        }
        return true;
      }
    }
    return false;
  }

  // Register a listener to a query, returning true if it was registered. If the
  // given listener is already registered to the given query, it will not be
  // registered again, and false will be returned.
  bool Register(const internal::QuerySpec& spec, T* listener) {
    MutexLock lock(mutex_);
    bool inserted = InsertIntoValueVectorAtKey(&listeners_, spec, listener);
    if (inserted) InsertIntoValueVectorAtKey(&listeners_rev_, listener, spec);
    return inserted;
  }
  // Unregister a listener from a query, returning true if the listener was
  // unregistered, false if it was not found.
  bool Unregister(const internal::QuerySpec& spec, T* listener) {
    MutexLock lock(mutex_);
    bool removed = RemoveFromValueVectorAtKey(&listeners_, spec, listener);
    if (removed) RemoveFromValueVectorAtKey(&listeners_rev_, listener, spec);
    return removed;
  }
  // Unregister all listeners from the given query.
  void UnregisterAll(const internal::QuerySpec& spec) {
    MutexLock lock(mutex_);
    auto i = listeners_.find(spec);
    if (i != listeners_.end()) {
      for (auto j = i->second.begin(); j != i->second.end(); ++j) {
        RemoveFromValueVectorAtKey(listeners_rev_, *j, spec);
      }
      listeners_.erase(i);
    }
  }
  // Unregister the given listener from all its queries.
  void UnregisterAll(T* listener) {
    MutexLock lock(mutex_);
    auto i = listeners_rev_.find(listener);
    if (i != listeners_rev_.end()) {
      for (auto j = i->second.begin(); j != i->second.end(); ++j) {
        RemoveFromValueVectorAtKey(listeners_, *j, listener);
      }
      listeners_rev_.erase(i);
    }
  }

  // Return all listeners for a given query.
  bool Get(const internal::QuerySpec& spec, std::vector<T*>* listeners_out) {
    MutexLock lock(mutex_);
    auto i = listeners_.find(spec);
    if (i != listeners_.end()) {
      if (listeners_out) *listeners_out = i->second;
      return true;
    }
    return false;
  }

  bool Exists(internal::QuerySpec spec) {
    return listeners_.find(spec) != listeners_.end();
  }

  bool Exists(T* listener) {
    return listeners_rev_.find(listener) != listeners_rev_.end();
  }

 private:
  Mutex mutex_;
  std::map<internal::QuerySpec, std::vector<T*>> listeners_;
  std::map<T*, std::vector<internal::QuerySpec>> listeners_rev_;
};

}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_LISTENER_H_
