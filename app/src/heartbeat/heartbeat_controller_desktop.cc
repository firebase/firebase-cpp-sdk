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

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
const char* kVersionValue = "2";
const char* kUserAgentKey = "agent";
const char* kDatesKey = "dates";
const int kMaxPayloadSize = 1024;
const int kMaxWaitTimeMs = 300;

// Minimum time between calls to GetPayload. Can be overridden in tests.
double g_min_time_between_fetches_sec = 30.0;

HeartbeatController::HeartbeatController(const std::string& app_id,
                                         const Logger& logger,
                                         const DateProvider& date_provider)
    : storage_(app_id, logger),
      date_provider_(date_provider),
      last_logged_date_(""),
      scheduler_() {}

HeartbeatController::~HeartbeatController() {}

void HeartbeatController::LogHeartbeat() {
  const char* ua_cstr = App::GetUserAgent();
  std::string user_agent = ua_cstr ? ua_cstr : "";
  std::function<void(void)> log_heartbeat_funct = [this, user_agent]() {
    std::string current_date = date_provider_.GetDate();
    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      // Stop early if the in-memory last_logged date is later than today, or
      // today for the same user agent.
      if (!this->last_logged_date_.empty() &&
          (this->last_logged_date_ > current_date ||
           (this->last_logged_date_ == current_date &&
            this->last_logged_user_agent_ == user_agent))) {
        return;
      }
    }
    LoggedHeartbeats logged_heartbeats;
    bool read_succeeded = this->storage_.ReadTo(logged_heartbeats);
    // If read fails, don't attempt to write. Note that corrupt or nonexistent
    // data should return an empty heartbeat instance and indicate successful
    // read.
    if (!read_succeeded) {
      return;
    }
    // Stop early if the stored last_logged date is later than today.
    if (!logged_heartbeats.last_logged_date.empty() &&
        logged_heartbeats.last_logged_date > current_date) {
      std::lock_guard<std::mutex> lock(this->mutex_);
      this->last_logged_date_ = logged_heartbeats.last_logged_date;
      return;
    }
    // If a heartbeat was already logged today, stop early if heartbeats were
    // already flushed or if this user agent already logged today.
    if (logged_heartbeats.last_logged_date == current_date) {
      {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->last_logged_date_ = logged_heartbeats.last_logged_date;
      }
      if (logged_heartbeats.heartbeats.empty()) {
        return;
      }
      auto it = logged_heartbeats.heartbeats.find(user_agent);
      if (it != logged_heartbeats.heartbeats.end() &&
          std::find(it->second.begin(), it->second.end(), current_date) !=
              it->second.end()) {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->last_logged_user_agent_ = user_agent;
        return;
      }
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
      std::lock_guard<std::mutex> lock(this->mutex_);
      this->last_logged_date_ = current_date;
      this->last_logged_user_agent_ = user_agent;
    }
  };

  scheduler_.Schedule(log_heartbeat_funct);
}

std::string HeartbeatController::GetAndResetStoredHeartbeats() {
  std::time_t now = 0;
  std::time(&now);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    double diff_s = std::difftime(now, last_read_all_heartbeats_time_);
    if (diff_s < g_min_time_between_fetches_sec) {
      return "";
    }
    last_read_all_heartbeats_time_ = now;
  }

  struct SharedFetchState {
    std::mutex mutex;
    bool timed_out = false;
    std::string output_str;
  };
  auto state = std::make_shared<SharedFetchState>();
  std::shared_ptr<Semaphore> scheduled_work_semaphore =
      std::make_shared<Semaphore>(0);

  std::function<void(void)> get_and_reset_function =
      [this, state, scheduled_work_semaphore]() {
        std::string current_date = date_provider_.GetDate();
        bool already_flushed = false;
        {
          std::lock_guard<std::mutex> lock(this->mutex_);
          already_flushed =
              (this->last_flushed_all_heartbeats_date_ == current_date);
        }
        // Return early if all heartbeats have already been fetched today.
        if (!already_flushed) {
          LoggedHeartbeats logged_heartbeats;
          bool read_succeeded = this->storage_.ReadTo(logged_heartbeats);
          // If read fails, or if there are no stored heartbeats, return an
          // empty payload and don't attempt to write.
          if (read_succeeded && !logged_heartbeats.heartbeats.empty()) {
            std::string compressed = CompressAndEncode(
                GetJsonPayloadForHeartbeats(logged_heartbeats));
            if (!compressed.empty()) {
              std::lock_guard<std::mutex> state_lock(state->mutex);
              if (!state->timed_out) {
                // Clear all logged heartbeats, but keep the last logged date
                LoggedHeartbeats cleared_heartbeats;
                cleared_heartbeats.last_logged_date =
                    logged_heartbeats.last_logged_date;
                bool write_succeeded = this->storage_.Write(cleared_heartbeats);
                // Only update last-flushed date and return a payload if the
                // write succeeds.
                if (write_succeeded) {
                  {
                    std::lock_guard<std::mutex> lock(this->mutex_);
                    this->last_flushed_all_heartbeats_date_ = current_date;
                  }
                  state->output_str = std::move(compressed);
                }
              }
            }
          }
        }
        // Post the semaphore to signal the main thread to read the result.
        scheduled_work_semaphore->Post();
      };

  scheduler_.Schedule(get_and_reset_function);
  // Wait until the scheduled work completes.
  if (scheduled_work_semaphore->TimedWait(kMaxWaitTimeMs)) {
    std::lock_guard<std::mutex> state_lock(state->mutex);
    return state->output_str;
  }
  {
    std::lock_guard<std::mutex> state_lock(state->mutex);
    state->timed_out = true;
    if (!state->output_str.empty()) {
      return state->output_str;
    }
  }
  // Return an empty string if TimedWait times out before completion.
  // TODO(b/239568581): Start an async process to wait for the completion of the
  // scheduled work and to store the result in memory for a later fetch.
  return "";
}

std::string HeartbeatController::GetAndResetTodaysStoredHeartbeats() {
  std::time_t now = 0;
  std::time(&now);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    double diff_s = std::difftime(now, last_read_todays_heartbeat_time_);
    if (diff_s < g_min_time_between_fetches_sec) {
      return "";
    }
    last_read_todays_heartbeat_time_ = now;
  }

  struct SharedFetchState {
    std::mutex mutex;
    bool timed_out = false;
    std::string output_str;
  };
  auto state = std::make_shared<SharedFetchState>();
  std::shared_ptr<Semaphore> scheduled_work_semaphore =
      std::make_shared<Semaphore>(0);

  std::function<void(void)> get_and_reset_function =
      [this, state, scheduled_work_semaphore]() {
        std::string current_date = date_provider_.GetDate();
        bool already_flushed = false;
        {
          std::lock_guard<std::mutex> lock(this->mutex_);
          already_flushed =
              (this->last_flushed_all_heartbeats_date_ == current_date ||
               this->last_flushed_todays_heartbeat_date_ == current_date);
        }
        // Return early if a heartbeat has already been fetched today.
        if (!already_flushed) {
          LoggedHeartbeats stored_heartbeats;
          bool read_succeeded = this->storage_.ReadTo(stored_heartbeats);
          // If read fails, or if there are no stored heartbeats, return an
          // empty payload and don't attempt to write.
          if (read_succeeded && !stored_heartbeats.heartbeats.empty()) {
            // Find a logged heartbeat from today. Remove it from the stored
            // heartbeats and use it to construct a single-heartbeat payload.
            std::string todays_heartbeats_user_agents;
            for (auto entry_it = stored_heartbeats.heartbeats.begin();
                 entry_it != stored_heartbeats.heartbeats.end();) {
              std::vector<std::string>& dates = entry_it->second;
              auto itr = std::find(dates.begin(), dates.end(), current_date);
              if (itr != dates.end()) {
                std::string user_agent = entry_it->first;
                dates.erase(itr);
                if (dates.empty()) {
                  entry_it = stored_heartbeats.heartbeats.erase(entry_it);
                } else {
                  ++entry_it;
                }
                todays_heartbeats_user_agents = std::move(user_agent);
                break;
              } else {
                ++entry_it;
              }
            }
            // Only write if a heartbeat was found for today.
            if (!todays_heartbeats_user_agents.empty()) {
              std::lock_guard<std::mutex> state_lock(state->mutex);
              if (!state->timed_out) {
                bool write_succeeded = this->storage_.Write(stored_heartbeats);
                // Only update last-flushed date and return a payload if the
                // write succeeds.
                if (write_succeeded) {
                  {
                    std::lock_guard<std::mutex> lock(this->mutex_);
                    this->last_flushed_todays_heartbeat_date_ = current_date;
                  }
                  state->output_str = std::move(todays_heartbeats_user_agents);
                }
              }
            }
          }
        }
        // Post the semaphore to signal the main thread to read the result.
        scheduled_work_semaphore->Post();
      };

  scheduler_.Schedule(get_and_reset_function);
  // Wait until the scheduled work completes.
  if (scheduled_work_semaphore->TimedWait(kMaxWaitTimeMs)) {
    std::lock_guard<std::mutex> state_lock(state->mutex);
    return state->output_str;
  }
  {
    std::lock_guard<std::mutex> state_lock(state->mutex);
    state->timed_out = true;
    if (!state->output_str.empty()) {
      return state->output_str;
    }
  }
  // Return an empty string if TimedWait times out before completion.
  return "";
}

std::string HeartbeatController::GetJsonPayloadForHeartbeats(
    const LoggedHeartbeats& heartbeats) {
  Variant heartbeats_variant = Variant::EmptyVector();
  std::vector<Variant>& heartbeats_vector = heartbeats_variant.vector();

  // heartbeats is a map from user agent to a vector of date strings.
  for (auto const& entry : heartbeats.heartbeats) {
    Variant heartbeat_entry = Variant::EmptyMap();
    std::map<Variant, Variant>& heartbeat_entry_map = heartbeat_entry.map();
    Variant dates_variant = Variant::EmptyVector();
    std::vector<Variant>& dates_vector = dates_variant.vector();
    dates_vector.reserve(entry.second.size());
    for (const std::string& date : entry.second) {
      dates_vector.push_back(date);
    }
    heartbeat_entry_map[kUserAgentKey] = entry.first;
    heartbeat_entry_map[kDatesKey] = std::move(dates_variant);
    heartbeats_vector.push_back(std::move(heartbeat_entry));
  }
  // Construct a final object with 'heartbeats' and 'version'
  Variant root = Variant::EmptyMap();
  std::map<Variant, Variant>& root_map = root.map();
  root_map[kHeartbeatsKey] = std::move(heartbeats_variant);
  root_map[kVersionKey] = kVersionValue;

  return util::VariantToJson(root);
}

std::string HeartbeatController::CompressAndEncode(const std::string& input) {
  ZLib zlib;
  zlib.SetGzipHeaderMode();
  uLongf result_size = ZLib::MinCompressbufSize(input.length());
  std::string compressed_str(result_size, '\0');
  int err = zlib.Compress(
      reinterpret_cast<Bytef*>(&compressed_str[0]), &result_size,
      reinterpret_cast<const Bytef*>(input.data()), input.length());
  if (err != Z_OK) {
    // Failed to compress.
    return "";
  }
  compressed_str.resize(result_size);

  std::string output;
  if (!firebase::internal::Base64EncodeUrlSafe(compressed_str, &output)) {
    // Failed to base64 encode.
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
  uLongf buffer_size = ZLib::MinCompressbufSize(decoded.length());
  if (buffer_size < 256) {
    buffer_size = 256;
  }
  const uLongf kMaxDecompressSize = 1024 * 1024;
  while (buffer_size <= kMaxDecompressSize) {
    uLongf result_size = buffer_size;
    std::string output(result_size, '\0');
    int err = zlib.Uncompress(
        reinterpret_cast<unsigned char*>(&output[0]), &result_size,
        reinterpret_cast<const unsigned char*>(decoded.data()),
        decoded.length());
    if (err == Z_OK) {
      output.resize(result_size);
      return output;
    }
    if (err != Z_BUF_ERROR) {
      break;
    }
    buffer_size *= 2;
  }
  // Failed to uncompress.
  return "";
}

}  // namespace heartbeat
}  // namespace firebase
