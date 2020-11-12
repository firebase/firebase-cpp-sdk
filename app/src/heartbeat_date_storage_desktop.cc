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

#include "app/src/heartbeat_date_storage_desktop.h"

#include <fstream>
#include <utility>

#include "app/src/filesystem.h"

namespace FIREBASE_NAMESPACE {

namespace {

const char kHeartbeatDir[] = "firebase-heartbeat";
const char kHeartbeatFilename[] = "HEARTBEAT_INFO_STORAGE";

// Returns the mutex that protects accesses to the storage file.
std::mutex& FileMutex() {
  static std::mutex* mutex_ = new std::mutex();
  return *mutex_;
}

std::string GetFilename(std::string& error) {
  std::string app_dir =
      AppDataDir(kHeartbeatDir, /*should_create=*/true, &error);
  if (app_dir.empty()) {
    return "";
  }

  return app_dir + "/" + kHeartbeatFilename;
}

}  // namespace

HeartbeatDateStorage::HeartbeatDateStorage() : filename_(GetFilename(error_)) {
  std::lock_guard<std::mutex> lock(FileMutex());

  // Ensure the file exists, otherwise the first attempt to read it would
  // fail.
  std::ofstream f(filename_, std::ios_base::app);

  if (!f) {
    std::string error = "Unable to open '" + filename_ + "'.";
    if (error_.empty()) {
      error_ = std::move(error);
    } else {
      error_ += "; ";
      error_ += error;
    }
  }
}

bool HeartbeatDateStorage::ReadPersisted() {
  if (!IsValid()) return false;

  HeartbeatMap result;

  std::lock_guard<std::mutex> lock(FileMutex());

  std::ifstream f(filename_);
  if (!f) {
    error_ = "Unable to open '" + filename_ + "' for reading.";
    return false;
  }

  while (!f.eof()) {
    std::string tag;
    f >> tag;
    if (f.eof()) {
      break;  // Will happen upon attempting to read an empty file.
    }

    std::time_t last_sent = 0;
    f >> last_sent;
    if (!f) {
      error_ = std::string("Error reading from '") + filename_ + "'.";
      return false;
    }

    result[tag] = last_sent;
  }

  heartbeat_map_ = std::move(result);

  return true;
}

bool HeartbeatDateStorage::WritePersisted() const {
  if (!IsValid()) return false;

  std::lock_guard<std::mutex> lock(FileMutex());

  std::ofstream f(filename_, std::ios_base::trunc);
  if (!f) {
    error_ = std::string("Unable to open '") + filename_ + "' for writing.";
    return false;
  }

  for (const auto& kv : heartbeat_map_) {
    f << kv.first << " " << kv.second << '\n';
  }

  return static_cast<bool>(f);
}

std::time_t HeartbeatDateStorage::Get(const std::string& tag) const {
  auto found = heartbeat_map_.find(tag);
  if (found != heartbeat_map_.end()) {
    return found->second;
  }

  return 0;
}

void HeartbeatDateStorage::Set(const std::string& tag, std::time_t last_sent) {
  heartbeat_map_[tag] = last_sent;
}

}  // namespace FIREBASE_NAMESPACE
