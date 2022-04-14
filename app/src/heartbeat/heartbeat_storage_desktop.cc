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

#include "app/src/heartbeat/heartbeat_storage_desktop.h"

#include <fstream>

#include "app/logged_heartbeats_generated.h"
#include "app/src/filesystem.h"
#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {
namespace heartbeat {

using com::google::firebase::cpp::heartbeat::GetLoggedHeartbeats;
using com::google::firebase::cpp::heartbeat::LoggedHeartbeats;
using com::google::firebase::cpp::heartbeat::VerifyLoggedHeartbeatsBuffer;

// Using an anonymous namespace for file mutex and filename
namespace {

const char kHeartbeatDir[] = "firebase-heartbeat";
const char kHeartbeatFilename[] = "HEARTBEAT_STORAGE";

// Returns the mutex that protects accesses to the storage file.
Mutex& FileMutex() {
  static Mutex* mutex_ = new Mutex(Mutex::kModeNonRecursive);
  return *mutex_;
}

std::string GetFilename(std::string& error) {
  std::string app_dir =
      AppDataDir(kHeartbeatDir, /*should_create=*/true, &error);
  if (app_dir.empty()) {
    return "";
  }

  // TODO(almostmatt): does this need to be different between windows/mac/linux?
  return app_dir + "/" + kHeartbeatFilename;
}

}  // namespace

HeartbeatStorageDesktop::HeartbeatStorageDesktop()
    : filename_(GetFilename(error_)) {
  MutexLock lock(FileMutex());

  // Ensure the file exists, otherwise the first attempt to read it would
  // fail.
  std::ofstream file(filename_, std::ios_base::app);

  if (!file) {
    error_ = "Unable to open '" + filename_ + "'.";
  }
}

const LoggedHeartbeats* HeartbeatStorageDesktop::Read() {
  MutexLock lock(FileMutex());
  error_ = "";

  // Open the file and seek to the end
  std::ifstream file(filename_, std::ios_base::binary | std::ios_base::ate);
  if (!file) {
    // TODO(almostmatt): if file does not exist, maybe return a default
    // instance.
    error_ = "Unable to open '" + filename_ + "' for reading.";
    return nullptr;
  }
  // Current position in file is buffer size.
  std::streamsize buffer_len = file.tellg();
  // Rewind to start of the file, then read into a buffer.
  file.seekg(0, std::ios_base::beg);
  char buffer[buffer_len];
  file.read(buffer, buffer_len);
  // Verify that the buffer is a valid flatbuffer.
  ::flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(buffer),
                                   buffer_len);
  if (!VerifyLoggedHeartbeatsBuffer(verifier)) {
    // TODO(almostmatt): maybe delete the file if it contains corrupted data
    error_ =
        "Failed to parse contents of " + filename_ + " as LoggedHeartbeats.";
    return nullptr;
  }
  return GetLoggedHeartbeats(&buffer);
}

bool HeartbeatStorageDesktop::Write(const LoggedHeartbeats* heartbeats) const {
  MutexLock lock(FileMutex());
  error_ = "";

  // Clear the file before writing.
  std::ofstream file(filename_, std::ios_base::trunc | std::ios_base::binary);
  if (!file) {
    error_ = "Unable to open '" + filename_ + "' for writing.";
    return false;
  }

  // LoggedHeartbeats is just part of a flatbuffer. To get the entire buffer,
  // Create a new flat buffer builder based on the LoggedHeartbeats instance.
  // TODO(almostmatt): This does not seem good. Find a better way to write.
  flatbuffers::FlatBufferBuilder fbb(1024);
  auto heartbeats_unique_ptr = UnPackLoggedHeartbeats(heartbeats);
  auto heartbeats_offset =
      CreateLoggedHeartbeats(fbb, heartbeats_unique_ptr.get());
  fbb.Finish(heartbeats_offset);
  file.write((char*)fbb.GetBufferPointer(), fbb.GetSize());

  return !file.fail();
}

}  // namespace heartbeat
}  // namespace firebase