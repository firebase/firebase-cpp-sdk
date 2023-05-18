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
#include "util/locate_emulator.h"

#include <string>

#include "app/src/assert.h"

namespace firebase {
namespace firestore {

// Set Firestore up to use Firestore Emulator via USE_FIRESTORE_EMULATOR
void LocateEmulator(Firestore* db) {
  // Use emulator as long as this env variable is set, regardless its value.
  if (std::getenv("USE_FIRESTORE_EMULATOR") == nullptr) {
    LogDebug("Using Firestore Prod for testing.");
    return;
  }

#if defined(__ANDROID__)
  // Special IP to access the hosting OS from Android Emulator.
  std::string local_host = "10.0.2.2";
#else
  std::string local_host = "localhost";
#endif  // defined(__ANDROID__)

  // Use FIRESTORE_EMULATOR_PORT if it is set to non empty string,
  // otherwise use the default port.
  std::string port = std::getenv("FIRESTORE_EMULATOR_PORT")
                         ? std::getenv("FIRESTORE_EMULATOR_PORT")
                         : "8080";
  std::string address =
      port.empty() ? (local_host + ":8080") : (local_host + ":" + port);

  LogInfo("Using Firestore Emulator (%s) for testing.", address.c_str());
  auto settings = db->settings();
  settings.set_host(address);
  // Emulator does not support ssl yet.
  settings.set_ssl_enabled(false);
  db->set_settings(settings);
}

}  // namespace firestore
}  // namespace firebase
