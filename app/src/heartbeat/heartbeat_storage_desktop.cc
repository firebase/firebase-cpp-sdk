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

#if FIREBASE_PLATFORM_WINDOWS
#include <windows.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
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

bool IsInvalidFilenameChar(char c) {
  constexpr char kInvalidFilenameChars[] = "/\\?%*:|\"<>.,;=";
  for (size_t i = 0; i < sizeof(kInvalidFilenameChars) - 1; ++i) {
    if (c == kInvalidFilenameChars[i]) {
      return true;
    }
  }
  return false;
}

#if FIREBASE_PLATFORM_WINDOWS
std::wstring CreateFilename(const std::string& app_id, const Logger& logger) {
  const std::wstring empty_string;
#else
std::string CreateFilename(const std::string& app_id, const Logger& logger) {
  const std::string empty_string;
#endif  // FIREBASE_PLATFORM_WINDOWS
  std::string error;
  std::string app_dir =
      AppDataDir(kHeartbeatDir, /*should_create=*/true, &error);
  if (!error.empty()) {
    logger.LogError("%s", error.c_str());
    return empty_string;
  }
  if (app_dir.empty()) {
    return empty_string;
  }

  // Remove any symbols from app_id that might not be allowed in filenames.
  std::string app_id_without_symbols;
  app_id_without_symbols.reserve(app_id.size());
  for (char c : app_id) {
    if (!IsInvalidFilenameChar(c)) {
      app_id_without_symbols.push_back(c);
    }
  }
  if (app_id_without_symbols != app_id) {
    // Disambiguate app_ids that differ only in stripped punctuation while
    // keeping app_id_without_symbols as a substring.
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : app_id) {
      hash ^= c;
      hash *= 1099511628211ULL;
    }
    std::ostringstream hash_ss;
    hash_ss << "-" << std::hex << hash;
    app_id_without_symbols += hash_ss.str();
  }
  // Note: fstream will convert / to \ if needed on windows.
  std::string final_path_utf8 =
      app_dir + "/" + kHeartbeatFilenamePrefix + app_id_without_symbols;
#if FIREBASE_PLATFORM_WINDOWS
  if (final_path_utf8.empty()) {
    return empty_string;
  }
  int wide_len = ::MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, final_path_utf8.data(),
      static_cast<int>(final_path_utf8.size()), nullptr, 0);
  if (wide_len <= 0) {
    logger.LogError("Failed to convert heartbeat path to UTF-16 (error: %lu).",
                    static_cast<unsigned long>(::GetLastError()));
    return empty_string;
  }
  std::wstring final_path_w(static_cast<size_t>(wide_len), L'\0');
  int converted_len = ::MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, final_path_utf8.data(),
      static_cast<int>(final_path_utf8.size()), &final_path_w[0], wide_len);
  if (converted_len != wide_len) {
    logger.LogError("Failed to convert heartbeat path to UTF-16.");
    return empty_string;
  }
  return final_path_w;
#else
  return final_path_utf8;
#endif
}

#if FIREBASE_PLATFORM_WINDOWS
#define FIREBASE_PATH_FMT "%ls"
#else
#define FIREBASE_PATH_FMT "%s"
#endif

}  // namespace
HeartbeatStorageDesktop::HeartbeatStorageDesktop(const std::string& app_id,
                                                 const Logger& logger)
    : filename_(CreateFilename(app_id, logger)), logger_(logger) {
  if (filename_.empty()) {
    return;
  }
  // Ensure the file exists, otherwise the first attempt to read it would
  // fail.
  std::ofstream file(filename_, std::ios_base::app);

  if (!file) {
    logger_.LogError("Unable to open '" FIREBASE_PATH_FMT "'.",
                     filename_.c_str());
  }
}

// Max size is arbitrary, just making sure that there is a sane limit.
static const int kMaxBufferSize = 1024 * 500;

bool HeartbeatStorageDesktop::ReadTo(LoggedHeartbeats& heartbeats_output) {
  if (filename_.empty()) {
    return false;
  }
  // Open the file and seek to the end
  std::ifstream file(filename_, std::ios_base::binary | std::ios_base::ate);
  if (!file) {
    logger_.LogError("Unable to open '" FIREBASE_PATH_FMT "' for reading.",
                     filename_.c_str());
    return false;
  }
  // Current position in file is file size. Fail if file is too large.
  std::streamsize buffer_len = file.tellg();
  if (buffer_len > kMaxBufferSize) {
    logger_.LogError("'" FIREBASE_PATH_FMT "' is too large to read.",
                     filename_.c_str());
    return false;
  }
  if (buffer_len < 0) {
    logger_.LogError("Failed to determine size of '" FIREBASE_PATH_FMT "'.",
                     filename_.c_str());
    return false;
  }
  if (buffer_len == 0) {
    heartbeats_output = LoggedHeartbeats();
    return true;
  }
  // Rewind to start of the file, then read into a buffer on the heap.
  file.seekg(0, std::ios_base::beg);
  std::string buffer(static_cast<size_t>(buffer_len), '\0');
  if (!file.read(&buffer[0], buffer_len) || file.gcount() != buffer_len) {
    logger_.LogError("Failed to read '" FIREBASE_PATH_FMT "'.",
                     filename_.c_str());
    return false;
  }
  // Verify that the buffer is a valid flatbuffer.
  ::flatbuffers::Verifier verifier(
      reinterpret_cast<const uint8_t*>(buffer.data()),
      static_cast<size_t>(buffer_len));
  if (!VerifyLoggedHeartbeatsBuffer(verifier)) {
    // If the file is empty or contains corrupted data, return a default
    // instance.
    heartbeats_output = LoggedHeartbeats();
    return true;
  }
  const LoggedHeartbeatsFlatbuffer* heartbeats_fb =
      GetLoggedHeartbeats(buffer.data());
  if (heartbeats_fb == nullptr) {
    heartbeats_output = LoggedHeartbeats();
    return true;
  }
  heartbeats_output = LoggedHeartbeatsFromFlatbuffer(*heartbeats_fb);
  return true;
}

#if FIREBASE_PLATFORM_WINDOWS
std::wstring HeartbeatStorageDesktop::GetFilename() const { return filename_; }
#else
std::string HeartbeatStorageDesktop::GetFilename() const { return filename_; }
#endif  // FIREBASE_PLATFORM_WINDOWS

bool HeartbeatStorageDesktop::Write(const LoggedHeartbeats& heartbeats) const {
  if (filename_.empty()) {
    return false;
  }
  // Clear the file before writing.
  std::ofstream file(filename_, std::ios_base::trunc | std::ios_base::binary);
  if (!file) {
    logger_.LogError("Unable to open '" FIREBASE_PATH_FMT "' for writing.",
                     filename_.c_str());
    return false;
  }

  flatbuffers::FlatBufferBuilder fbb = LoggedHeartbeatsToFlatbuffer(heartbeats);
  file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
             fbb.GetSize());
  file.flush();
  file.close();

  return !file.fail();
}

LoggedHeartbeats HeartbeatStorageDesktop::LoggedHeartbeatsFromFlatbuffer(
    const LoggedHeartbeatsFlatbuffer& heartbeats_fb) const {
  LoggedHeartbeats heartbeats_struct;
  // TODO(almostmatt): verify format of date string
  if (heartbeats_fb.last_logged_date() != nullptr) {
    heartbeats_struct.last_logged_date =
        heartbeats_fb.last_logged_date()->str();
  }
  if (heartbeats_fb.heartbeats() != nullptr) {
    for (auto user_agent_and_dates : *(heartbeats_fb.heartbeats())) {
      if (user_agent_and_dates == nullptr ||
          user_agent_and_dates->user_agent() == nullptr) {
        continue;
      }
      std::string user_agent = user_agent_and_dates->user_agent()->str();
      if (user_agent_and_dates->dates() != nullptr) {
        for (auto date : *(user_agent_and_dates->dates())) {
          if (date == nullptr) {
            continue;
          }
          // TODO(almostmatt): verify format of date string
          heartbeats_struct.heartbeats[user_agent].push_back(date->str());
        }
      }
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
