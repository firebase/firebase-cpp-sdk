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

#include "app/src/heartbeat_info_desktop.h"

#include "app/src/heartbeat_date_storage_desktop.h"
#include "app/src/log.h"

namespace FIREBASE_NAMESPACE {
namespace {

const std::time_t kHeartbeatIntervalSeconds = 86400;  // 24 hours
const char kHeartbeatGlobalTag[] = "GLOBAL";

// Heartbeat may be invoked many times consecutively; make sure to only log an
// error once to avoid spamming the log.
void LogOnce(const std::string& message) {
  static bool error_logged = false;
  if (!error_logged) {
    error_logged = true;
    LogInfo(message.c_str());
  }
}

// Returns whether the heartbeat for the SDK referred to by `tag` needs to be
// sent to the backend.
bool CheckAndUpdateHeartbeatTime(const char* tag,
                                 HeartbeatDateStorage& storage) {
  // Will return 0 if `tag` isn't in the map yet.
  std::time_t last_heartbeat_seconds = storage.Get(tag);

  std::time_t current_time_seconds = 0;
  std::time(&current_time_seconds);
  if (current_time_seconds - last_heartbeat_seconds <
      kHeartbeatIntervalSeconds) {
    return false;
  }

  storage.Set(tag, current_time_seconds);

  return true;
}

bool ReadFromStorage(HeartbeatDateStorage& storage) {
  bool ok = storage.ReadPersisted();
  if (!ok) {
    LogOnce("Heartbeat failed: unable to read the heartbeat data");
  }
  return ok;
}

bool WriteToStorage(const HeartbeatDateStorage& storage) {
  bool ok = storage.WritePersisted();
  if (!ok) {
    LogOnce("Heartbeat failed: unable to write the heartbeat data");
  }
  return ok;
}

}  // namespace

HeartbeatInfo::Code HeartbeatInfo::GetHeartbeatCode(const char* tag) {
  HeartbeatDateStorage storage;

  bool read_ok = ReadFromStorage(storage);
  if (!read_ok) {
    return Code::None;
  }

  bool send_sdk = CheckAndUpdateHeartbeatTime(tag, storage);
  bool send_global = CheckAndUpdateHeartbeatTime(kHeartbeatGlobalTag, storage);

  bool write_ok = WriteToStorage(storage);
  if (!write_ok) {
    return Code::None;
  }

  if (!send_sdk && !send_global) {
    return Code::None;
  } else if (send_sdk && !send_global) {
    return Code::Sdk;
  } else if (!send_sdk && send_global) {
    return Code::Global;
  } else {
    return Code::Combined;
  }
}

}  // namespace FIREBASE_NAMESPACE
