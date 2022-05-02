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
#include <regex>
#include <vector>

#include "app/logged_heartbeats_generated.h"
#include "app/src/filesystem.h"
#include "app/src/logger.h"

namespace firebase {
namespace heartbeat {

using LoggedHeartbeatsFlatbuffer =
    com::google::firebase::cpp::heartbeat::LoggedHeartbeats;
using com::google::firebase::cpp::heartbeat::CreateLoggedHeartbeats;
using com::google::firebase::cpp::heartbeat::CreateUserAgentAndDates;
using com::google::firebase::cpp::heartbeat::GetLoggedHeartbeats;
using com::google::firebase::cpp::heartbeat::UserAgentAndDates;
using com::google::firebase::cpp::heartbeat::VerifyLoggedHeartbeatsBuffer;

// Using an anonymous namespace for helper to construct filename
namespace {

const char kHeartbeatDir[] = "firebase-heartbeat";
const char kHeartbeatFilenamePrefix[] = "heartbeats-";

std::string CreateFilename(const std::string& app_id, const Logger& logger) {
  std::string error;
  std::string app_dir =
      AppDataDir(kHeartbeatDir, /*should_create=*/true, &error);
  if (!error.empty()) {
    logger.LogError(error.c_str());
    return "";
  }
  if (app_dir.empty()) {
    return "";
  }

  // Remove any symbols from app_id that might not be allowed in filenames.
  auto app_id_without_symbols =
      std::regex_replace(app_id, std::regex("[/\\\\?%*:|\"<>.,;=]"), "");
  // Note: fstream will convert / to \ if needed on windows.
  return app_dir + "/" + kHeartbeatFilenamePrefix + app_id_without_symbols;
}

}  // namespace
HeartbeatStorageDesktop::HeartbeatStorageDesktop(const std::string& app_id,
                                                 const Logger& logger)
    : filename_(CreateFilename(app_id, logger)), logger_(logger) {
  // Ensure the file exists, otherwise the first attempt to read it would
  // fail.
  std::ofstream file(filename_, std::ios_base::app);

  if (!file) {
    logger_.LogError("Unable to open '%s'.", filename_.c_str());
  }
}

// Max size is arbitrary, just making sure that there is a sane limit.
static const int kMaxBufferSize = 1024 * 500;

bool HeartbeatStorageDesktop::ReadTo(LoggedHeartbeats& heartbeats_output) {
  // Open the file and seek to the end
  std::ifstream file(filename_, std::ios_base::binary | std::ios_base::ate);
  if (!file) {
    logger_.LogError("Unable to open '%s' for reading.", filename_.c_str());
    return false;
  }
  // Current position in file is file size. Fail if file is too large.
  std::streamsize buffer_len = file.tellg();
  if (buffer_len > kMaxBufferSize) {
    logger_.LogError("'%s' is too large to read.", filename_.c_str());
    return false;
  }
  if (buffer_len == -1) {
    logger_.LogError("Failed to determine size of '%s'.", filename_.c_str());
    return false;
  }
  // Rewind to start of the file, then read into a buffer on the heap.
  file.seekg(0, std::ios_base::beg);
  char* buffer = new char[buffer_len];
  file.read(buffer, buffer_len);
  // Verify that the buffer is a valid flatbuffer.
  ::flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(buffer),
                                   buffer_len);
  if (!VerifyLoggedHeartbeatsBuffer(verifier)) {
    // If the file is empty or contains corrupted data, return a default
    // instance.
    heartbeats_output = LoggedHeartbeats();
    delete[] buffer;
    return true;
  }
  const LoggedHeartbeatsFlatbuffer* heartbeats_fb = GetLoggedHeartbeats(buffer);
  heartbeats_output = LoggedHeartbeatsFromFlatbuffer(*heartbeats_fb);
  delete[] buffer;
  return true;
}

bool HeartbeatStorageDesktop::Write(const LoggedHeartbeats& heartbeats) const {
  // Clear the file before writing.
  std::ofstream file(filename_, std::ios_base::trunc | std::ios_base::binary);
  if (!file) {
    logger_.LogError("Unable to open '%s' for writing.", filename_.c_str());
    return false;
  }

  flatbuffers::FlatBufferBuilder fbb = LoggedHeartbeatsToFlatbuffer(heartbeats);
  file.write((char*)fbb.GetBufferPointer(), fbb.GetSize());

  return !file.fail();
}

const char* HeartbeatStorageDesktop::GetFilename() const {
  return filename_.c_str();
}

LoggedHeartbeats HeartbeatStorageDesktop::LoggedHeartbeatsFromFlatbuffer(
    const LoggedHeartbeatsFlatbuffer& heartbeats_fb) const {
  LoggedHeartbeats heartbeats_struct;
  // TODO(almostmatt): verify format of date string
  heartbeats_struct.last_logged_date = heartbeats_fb.last_logged_date()->str();
  for (auto user_agent_and_dates : *(heartbeats_fb.heartbeats())) {
    std::string user_agent = user_agent_and_dates->user_agent()->str();
    for (auto date : *(user_agent_and_dates->dates())) {
      // TODO(almostmatt): verify format of date string
      heartbeats_struct.heartbeats[user_agent].push_back(date->str());
    }
  }
  return heartbeats_struct;
}

flatbuffers::FlatBufferBuilder
HeartbeatStorageDesktop::LoggedHeartbeatsToFlatbuffer(
    const LoggedHeartbeats& heartbeats_struct) const {
  flatbuffers::FlatBufferBuilder builder;
  auto last_logged_date =
      builder.CreateString(heartbeats_struct.last_logged_date);

  std::vector<flatbuffers::Offset<UserAgentAndDates>> agents_and_dates_vector;
  for (auto const& entry : heartbeats_struct.heartbeats) {
    auto user_agent = builder.CreateString(entry.first);
    std::vector<flatbuffers::Offset<flatbuffers::String>> dates_vector;
    for (auto date : entry.second) {
      dates_vector.push_back(builder.CreateString(date));
    }
    auto agent_and_dates = CreateUserAgentAndDates(
        builder, user_agent, builder.CreateVector(dates_vector));
    agents_and_dates_vector.push_back(agent_and_dates);
  }
  auto loggedHeartbeats = CreateLoggedHeartbeats(
      builder, last_logged_date, builder.CreateVector(agents_and_dates_vector));
  builder.Finish(loggedHeartbeats);
  return builder;
}

}  // namespace heartbeat
}  // namespace firebase
