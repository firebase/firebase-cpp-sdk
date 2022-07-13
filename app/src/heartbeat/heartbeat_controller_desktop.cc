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

#include "app/src/heartbeat/heartbeat_controller_desktop.h"

#include <chrono>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "app/memory/shared_ptr.h"
#include "app/rest/zlibwrapper.h"
#include "app/src/app_common.h"
#include "app/src/base64.h"
#include "app/src/heartbeat/date_provider.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/logger.h"
#include "app/src/semaphore.h"
#include "app/src/variant_util.h"

namespace firebase {
namespace heartbeat {

// Named keys in the generated JSON payload
const char* kHeartbeatsKey = "heartbeats";
const char* kVersionKey = "version";
const char* kUserAgentKey = "agent";
const char* kDatesKey = "dates";
const int kMaxPayloadSize = 1024;
const int kMaxWaitTimeMs = 3000;

HeartbeatController::HeartbeatController(const std::string& app_id,
                                         const Logger& logger,
                                         const DateProvider& date_provider)
    : storage_(app_id, logger),
      scheduler_(),
      last_logged_date_(""),
      date_provider_(date_provider) {}

HeartbeatController::~HeartbeatController() {}

void HeartbeatController::LogHeartbeat() {
  std::function<void(void)> log_heartbeat_funct = [&]() {
    std::string user_agent = App::GetUserAgent();
    std::string current_date = date_provider_.GetDate();
    // Stop early if the in-memory last_logged date is today or later.
    if (!this->last_logged_date_.empty() &&
        this->last_logged_date_ >= current_date) {
      return;
    }
    LoggedHeartbeats logged_heartbeats;
    bool read_succeeded = this->storage_.ReadTo(logged_heartbeats);
    // If read fails, don't attempt to write. Note that corrupt or nonexistent
    // data should return an empty heartbeat instance and indicate successful
    // read.
    if (!read_succeeded) {
      return;
    }
    // Stop early if the stored last_logged date is today or later.
    if (!logged_heartbeats.last_logged_date.empty() &&
        logged_heartbeats.last_logged_date >= current_date) {
      this->last_logged_date_ = logged_heartbeats.last_logged_date;
      return;
    }
    logged_heartbeats.last_logged_date = current_date;
    logged_heartbeats.heartbeats[user_agent].push_back(current_date);
    // Don't store more than 30 days for the same user agent.
    if (logged_heartbeats.heartbeats[user_agent].size() > 30) {
      logged_heartbeats.heartbeats[user_agent].erase(
          logged_heartbeats.heartbeats[user_agent].begin());
    }
    // TODO(b/237003018): Implement file-lock to prevent race conditions between
    // multiple instances of the controller or multiple threads.
    bool write_succeeded = this->storage_.Write(logged_heartbeats);
    // Only update last-logged date if the write succeeds.
    if (write_succeeded) {
      this->last_logged_date_ = current_date;
    }
  };

  scheduler_.Schedule(log_heartbeat_funct);
}

std::string HeartbeatController::GetAndResetStoredHeartbeats() {
  SharedPtr<std::string> output_str = MakeShared<std::string>("");
  SharedPtr<Semaphore> scheduled_work_semaphore = MakeShared<Semaphore>(0);

  std::function<void(void)> get_and_reset_function =
      [&, output_str, scheduled_work_semaphore]() {
        std::string current_date = date_provider_.GetDate();
        // Return early if all heartbeats have already been fetched today.
        if (this->last_fetched_all_heartbeats_date_ != current_date) {
          LoggedHeartbeats logged_heartbeats;
          bool read_succeeded = this->storage_.ReadTo(logged_heartbeats);
          // If read fails, or if there are no stored heartbeats, return an
          // empty payload and don't attempt to write.
          if (read_succeeded && logged_heartbeats.heartbeats.size() > 0) {
            // Clear all logged heartbeats, but keep the last logged date
            LoggedHeartbeats cleared_heartbeats;
            cleared_heartbeats.last_logged_date =
                logged_heartbeats.last_logged_date;
            bool write_succeeded = this->storage_.Write(cleared_heartbeats);
            // Only update last-logged date and return a payload if the write
            // succeeds.
            if (write_succeeded) {
              this->last_fetched_all_heartbeats_date_ = current_date;
              *output_str = GetStringPayloadForHeartbeats(logged_heartbeats);
            }
          }
        }
        // Post the semaphore to signal the main thread to read the result.
        scheduled_work_semaphore->Post();
        return;
      };

  scheduler_.Schedule(get_and_reset_function);
  // Wait until the scheduled work completes.
  if (scheduled_work_semaphore->TimedWait(kMaxWaitTimeMs)) {
    return *output_str;
  }
  // Return an empty string if unable to TimedWait times out.
  return "";
}

std::string HeartbeatController::GetAndResetTodaysStoredHeartbeats() {
  SharedPtr<std::string> output_str = MakeShared<std::string>("");
  SharedPtr<Semaphore> scheduled_work_semaphore = MakeShared<Semaphore>(0);

  std::function<void(void)> get_and_reset_function =
      [&, output_str, scheduled_work_semaphore]() {
        std::string current_date = date_provider_.GetDate();
        // Return early if a heartbeat has already been fetched today.
        if (this->last_fetched_all_heartbeats_date_ != current_date &&
            this->last_fetched_todays_heartbeat_date_ != current_date) {
          LoggedHeartbeats stored_heartbeats;
          bool read_succeeded = this->storage_.ReadTo(stored_heartbeats);
          // If read fails, or if there are no stored heartbeats, return an
          // empty payload and don't attempt to write.
          if (read_succeeded && stored_heartbeats.heartbeats.size() > 0) {
            // Find a logged heartbeat from today. Remove it from the stored
            // heartbeats and use it to construct a single-heartbeat payload.
            LoggedHeartbeats todays_heartbeats;
            for (auto& entry : stored_heartbeats.heartbeats) {
              std::string user_agent = entry.first;
              std::vector<std::string>& dates = entry.second;
              auto itr = std::find(dates.begin(), dates.end(), current_date);
              if (itr != dates.end()) {
                dates.erase(itr);
                todays_heartbeats.heartbeats[user_agent].push_back(
                    current_date);
              }
            }
            // Only write if a heartbeat was found for today.
            if (todays_heartbeats.heartbeats.size() != 0) {
              bool write_succeeded = this->storage_.Write(stored_heartbeats);
              // Only update last-logged date and return a payload if the write
              // succeeds.
              if (write_succeeded) {
                this->last_fetched_todays_heartbeat_date_ = current_date;
                *output_str = GetStringPayloadForHeartbeats(todays_heartbeats);
              }
            }
          }
        }
        // Post the semaphore to signal the main thread to read the result.
        scheduled_work_semaphore->Post();
        return;
      };

  scheduler_.Schedule(get_and_reset_function);
  // Wait until the scheduled work completes.
  if (scheduled_work_semaphore->TimedWait(kMaxWaitTimeMs)) {
    return *output_str;
  }
  // Return an empty string if unable to TimedWait times out.
  return "";
}

std::string HeartbeatController::GetStringPayloadForHeartbeats(
    LoggedHeartbeats heartbeats) {
  Variant heartbeats_variant = Variant::EmptyVector();
  std::vector<Variant>& heartbeats_vector = heartbeats_variant.vector();

  // heartbeats is a map from user agent to a vector of date strings.
  for (auto const& entry : heartbeats.heartbeats) {
    Variant heartbeat_entry = Variant::EmptyMap();
    std::map<Variant, Variant>& heartbeat_entry_map = heartbeat_entry.map();
    Variant dates_variant = Variant::EmptyVector();
    std::vector<Variant>& dates_vector = dates_variant.vector();
    std::string user_agent = entry.first;
    for (std::string date : entry.second) {
      dates_vector.push_back(date);
    }
    heartbeat_entry_map[kUserAgentKey] = user_agent;
    heartbeat_entry_map[kDatesKey] = dates_variant;
    heartbeats_vector.push_back(heartbeat_entry);
  }
  // Construct a final object with 'heartbeats' and 'version'
  Variant root = Variant::EmptyMap();
  std::map<Variant, Variant>& root_map = root.map();
  root_map[kHeartbeatsKey] = heartbeats_variant;
  root_map[kVersionKey] = "2";

  std::string json_object = util::VariantToJson(root);
  return CompressAndEncode(json_object);
}

std::string HeartbeatController::CompressAndEncode(const std::string& input) {
  ZLib zlib;
  zlib.SetGzipHeaderMode();
  uLongf result_size = ZLib::MinCompressbufSize(input.length());
  std::unique_ptr<char[]> result(new char[result_size]);
  int err = zlib.Compress(
      reinterpret_cast<unsigned char*>(result.get()), &result_size,
      reinterpret_cast<const unsigned char*>(input.data()), input.length());
  if (err != Z_OK) {
    // Failed to compress.
    return "";
  }
  std::string compressed_str = std::string(result.get(), result_size);

  std::string output;
  if (!firebase::internal::Base64EncodeUrlSafe(compressed_str, &output)) {
    // Failed to based64 encode.
    return "";
  }
  // If compressed payload is too large, return an empty string instead.
  if (output.size() > kMaxPayloadSize) {
    return "";
  }
  return output;
}

std::string HeartbeatController::DecodeAndDecompress(const std::string& input) {
  std::string decoded;
  if (!firebase::internal::Base64Decode(input, &decoded)) {
    // Failed to decode.
    return "";
  }

  ZLib zlib;
  zlib.SetGzipHeaderMode();
  uLongf result_size = ZLib::MinCompressbufSize(decoded.length());
  std::unique_ptr<char[]> result(new char[result_size]);
  int err = zlib.Uncompress(
      reinterpret_cast<unsigned char*>(result.get()), &result_size,
      reinterpret_cast<const unsigned char*>(decoded.data()), decoded.length());
  if (err == Z_OK) {
    return std::string(result.get(), result_size);
  }
  // Failed to uncompress.
  return "";
}

}  // namespace heartbeat
}  // namespace firebase
