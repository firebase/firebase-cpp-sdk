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

#include "app/src/heartbeat/date_provider.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace firebase {
namespace heartbeat {

std::string DateProviderImpl::GetDate() const {
  std::time_t t = std::time(nullptr);
  // Use UTC time so that local time zone changes are ignored.
  std::tm utc_tm{};
#if defined(_WIN32)
  gmtime_s(&utc_tm, &t);
#else
  gmtime_r(&t, &utc_tm);
#endif
  std::ostringstream ss;
  // Format the date as yyyy-mm-dd, independent of locale.
  ss << std::put_time(&utc_tm, "%Y-%m-%d");
  return ss.str();
}

}  // namespace heartbeat
}  // namespace firebase
