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
#include "app/src/logger.h"

namespace firebase {
namespace heartbeat {

using LoggedHeartbeatsFlatbuffer =
    com::google::firebase::cpp::heartbeat::LoggedHeartbeats;

struct LoggedHeartbeats {
  // Last date for which a heartbeat was logged (YYYY-MM-DD).
  std::string last_logged_date;

  // Map from user agent to a list of dates (YYYY-MM-DD).
  std::map<std::string, std::vector<std::string> > heartbeats;
};

class HeartbeatStorageDesktop {
 public:
  explicit HeartbeatStorageDesktop(const std::string& app_id,
                                   const Logger& logger);

  // Reads an instance of LoggedHeartbeats from disk into the provided struct.
  // Returns `false` if the read operation fails.
  bool ReadTo(LoggedHeartbeats& heartbeats_output);

  // Writes an instance of LoggedHeartbeats to disk. Returns `false` if the
  // write operation fails.
  bool Write(const LoggedHeartbeats& heartbeats) const;

  const char* GetFilename() const;

 private:
  LoggedHeartbeats LoggedHeartbeatsFromFlatbuffer(
      const LoggedHeartbeatsFlatbuffer& heartbeats_fb) const;
  flatbuffers::FlatBufferBuilder LoggedHeartbeatsToFlatbuffer(
      const LoggedHeartbeats& heartbeats_struct) const;

  // local variables for state
  std::string filename_;
  const Logger& logger_;
};

}  // namespace heartbeat
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_HEARTBEAT_HEARTBEAT_STORAGE_DESKTOP_H_
