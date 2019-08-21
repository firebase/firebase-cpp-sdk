/*
 * Copyright 2019 Google LLC
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

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <combaseapi.h>
#elif HAVE_LIBUUID
#include <uuid/uuid.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

#include "app/src/assert.h"
#include "app/src/uuid.h"

namespace firebase {
namespace internal {

void Uuid::Generate() {
#if FIREBASE_PLATFORM_WINDOWS
  FIREBASE_ASSERT(sizeof(GUID) == sizeof(data));
  HRESULT result = CoCreateGuid(reinterpret_cast<GUID*>(data));
  FIREBASE_ASSERT(result == S_OK);
  (void)result;
#elif HAVE_LIBUUID
  static_assert(sizeof(uuid_t) == sizeof(data));
  uuid_generate_time(reinterpret_cast<uuid_t>(data));
#else
  int file = open("/dev/urandom", O_RDONLY);
  FIREBASE_ASSERT(file >= 0);
  size_t bytes_read = read(file, data, sizeof(data));
  FIREBASE_ASSERT(bytes_read == sizeof(data));
  close(file);
#endif  // FIREBASE_PLATFORM_WINDOWS
}

}  // namespace internal
}  // namespace firebase
