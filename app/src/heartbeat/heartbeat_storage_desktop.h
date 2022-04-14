/*
 * Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_SRC_HEARTBEAT_STORAGE_DESKTOP_H_
#define FIREBASE_APP_SRC_HEARTBEAT_STORAGE_DESKTOP_H_

#include <ctime>
#include <map>
#include <string>

#include "app/logged_heartbeats_generated.h"

namespace firebase {
namespace heartbeat {

using com::google::firebase::cpp::heartbeat::LoggedHeartbeats;

// Reads and writes the last time heartbeat was sent for an SDK using persistent
// storage.
//
// As this class uses the filesystem, there are several potential points of
// failure:
// - In the case of a failure, `Read` will return nullptr and `Write` will
// return false;
// - If a failure occurred, GetError can be used to get a descriptive error
// message.
// TODO(almostmatt): consider returning distinct error codes.

class HeartbeatStorageDesktop {
 public:
  HeartbeatStorageDesktop();

  // If the previous disk operation failed, contains additional details about
  // the error; otherwise is empty.
  const std::string& GetError() const { return error_; }

  // Reads an instance of LoggedHeartbeats from disk. Returns `false` if the
  // read operation fails.
  const LoggedHeartbeats* Read();

  // Writes an instance of LoggedHeartbeats to disk. Returns `false` if the
  // write operation fails.
  bool Write(const LoggedHeartbeats* heartbeats) const;

 private:
  // local variables for state
  mutable std::string error_;
  std::string filename_;
};

}  // namespace heartbeat
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_HEARTBEAT_DATE_STORAGE_DESKTOP_H_
