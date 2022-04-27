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

#ifndef FIREBASE_APP_SRC_HEARTBEAT_HEARTBEAT_STORAGE_DESKTOP_H_
#define FIREBASE_APP_SRC_HEARTBEAT_HEARTBEAT_STORAGE_DESKTOP_H_

#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "app/logged_heartbeats_generated.h"

namespace firebase {
namespace heartbeat {

using LoggedHeartbeatsFbs =
    com::google::firebase::cpp::heartbeat::LoggedHeartbeats;

struct LoggedHeartbeats {
  // Last date for which a heartbeat was logged (YYYY-MM-DD).
  std::string last_logged_date;

  // Map from user agent to a list of dates (YYYY-MM-DD).
  std::map<std::string, std::vector<std::string> > heartbeats;
};

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
  explicit HeartbeatStorageDesktop(std::string app_id);

  // If the previous disk operation failed, contains additional details about
  // the error; otherwise is empty.
  const std::string& GetError() const { return error_; }

  // Reads an instance of LoggedHeartbeats from disk into the provided struct.
  // Returns `false` if the read operation fails.
  bool ReadTo(LoggedHeartbeats* heartbeats_output);

  // Writes an instance of LoggedHeartbeats to disk. Returns `false` if the
  // write operation fails.
  bool Write(LoggedHeartbeats heartbeats) const;

  /****
    struct LoggedHeartbeatsS {
      string
      vector or map
    }
    can use diff date format if needed, or verify when parsing
    struct for in-mem representation is nice for mutability and hiding
    flatbuffer schema benefit of flatbuffer becomes just the ability to verify
    contents could use scheduler (see user secure manager.cc) for controller,
    could be that load from disk happens async and some time later the result of
    load is sent to server
    ********/

 private:
  LoggedHeartbeats LoggedHeartbeatsFromFbs(
      const LoggedHeartbeatsFbs* heartbeats_fbs) const;
  flatbuffers::FlatBufferBuilder LoggedHeartbeatsToFbs(
      LoggedHeartbeats heartbeats_struct) const;

  // local variables for state
  mutable std::string error_;
  std::string filename_;
};

}  // namespace heartbeat
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_HEARTBEAT_HEARTBEAT_STORAGE_DESKTOP_H_
