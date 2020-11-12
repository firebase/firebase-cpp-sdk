/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_HEARTBEAT_DATE_STORAGE_DESKTOP_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_HEARTBEAT_DATE_STORAGE_DESKTOP_H_

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

#include <ctime>
#include <map>
#include <mutex>  // NOLINT(build/c++11)
#include <string>

namespace FIREBASE_NAMESPACE {

// Reads and writes the last time heartbeat was sent for an SDK using persistent
// storage.
//
// As this class uses the filesystem, there are several potential points of
// failure:
// - check the return values of `ReadPersisted` and `WritePersisted` -- they
//   indicate whether the corresponding disk operations finished successfully;
// - always call `ReadPersisted` before calling `Get` to make sure the internal
//   map is initialized.
class HeartbeatDateStorage {
 public:
  HeartbeatDateStorage();

  // If the previous disk operation failed, contains additional details about
  // the error; otherwise is empty.
  const std::string& GetError() const { return error_; }

  // Reads the persisted data from disk. Returns `false` if the read operation
  // failed. Always call before calling `Get`.
  //
  // If the read operations failed, the internal map stays unchanged.
  bool ReadPersisted();

  // Writes the persisted data to disk. Returns `false` if the write operation
  // failed. Always call after calling `Set` to persist the data.
  bool WritePersisted() const;

  std::time_t Get(const std::string& tag) const;
  void Set(const std::string& tag, std::time_t last_sent);

 private:
  bool IsValid() const { return error_.empty(); }

  // The storage format is very simple: a key and a value are separated by
  // spaces and map entries are separated by newlines, e.g.
  //
  // foo 1602797566
  // bar 1602797576
  // baz 1602797600

  using HeartbeatMap = std::map<std::string, std::time_t>;

  mutable std::string error_;
  std::string filename_;
  HeartbeatMap heartbeat_map_;
};

}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_HEARTBEAT_DATE_STORAGE_DESKTOP_H_
