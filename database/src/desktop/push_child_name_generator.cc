// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "database/src/desktop/push_child_name_generator.h"

#include <assert.h>

namespace firebase {
namespace database {
namespace internal {

const char* const PushChildNameGenerator::kPushChars =
    "-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

// This implementation of GeneratePushChildName is based on the C#
// implementation found here:
// //firebase/database/client/mono/Database/Internal/Utilities/PushIdGenerator.cs
std::string PushChildNameGenerator::GeneratePushChildName(int64_t now) {
  // Prevent two simultanious calls to this function, which may result in
  // identical names being generated
  MutexLock lock(mutex_);

  std::string result;
  result.reserve(kGeneratedNameLength);

  // If this function is called in rapid succession, the timestamp-based
  // portion of the name generation will be identical. In this case, the
  // non-timestamp-based portion of the name needs to be guarenteed to be
  // unique.
  bool duplicate_time = (now == last_push_time_);
  last_push_time_ = now;

  char timestamp_chars[kNumTimestampChars + 1] = {0};

  // Fill in the first 8 characters with 'random' characters based on the
  // current timestamp.
  for (int i = kNumTimestampChars - 1; i >= 0; i--) {
    timestamp_chars[i] = kPushChars[static_cast<int>(now % kNumPushChars)];
    now /= kNumPushChars;
  }
  result += timestamp_chars;
  assert(now == 0);

  if (!duplicate_time) {
    // Provides random numbers in the range [0, kNumPushChars - 1].
    std::uniform_int_distribution<int> rng_in_range(0, kNumPushChars - 1);

    // The timestamp is unique, so just append 12 random characters to the end
    // of the name.
    for (int& last_rand_char : last_rand_chars_) {
      last_rand_char = rng_in_range(random_device_);
    }
  } else {
    // The timestamp was not unique, so to guarentee uniqueness, take the
    // previous set of 12 characters, and increment it by 1.
    for (int i = kNumRandomChars - 1; i >= 0; i--) {
      if (last_rand_chars_[i] != (kNumPushChars - 1)) {
        last_rand_chars_[i] = last_rand_chars_[i] + 1;
        break;
      }
      last_rand_chars_[i] = 0;
    }
  }
  for (int last_rand_char : last_rand_chars_) {
    result += kPushChars[last_rand_char];
  }
  assert(result.size() == kGeneratedNameLength);

  return result;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
