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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PUSH_CHILD_NAME_GENERATOR_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PUSH_CHILD_NAME_GENERATOR_H_

#include <random>
#include <string>
#include "app/src/mutex.h"

namespace firebase {
namespace database {
namespace internal {

class PushChildNameGenerator {
 public:
  PushChildNameGenerator()
      : random_device_(), mutex_(), last_push_time_(0), last_rand_chars_() {}

  std::string GeneratePushChildName(int64_t now);

 private:
  static const char* const kPushChars;
  static constexpr int kNumPushChars = 64;
  static constexpr int kNumTimestampChars = 8;
  static constexpr int kNumRandomChars = 12;
  static constexpr int kGeneratedNameLength =
      kNumTimestampChars + kNumRandomChars;

  // For random number generation.
  std::random_device random_device_;

  // Prevent two simultanious calls to this function, which may result in
  // identical names being generated
  Mutex mutex_;

  // The timestamp of the last call to this function.
  int64_t last_push_time_;

  // The most recent set of random chars generated. This is kept to ensure that
  // the next set of random characters generated is unique.
  int last_rand_chars_[kNumRandomChars];
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PUSH_CHILD_NAME_GENERATOR_H_
