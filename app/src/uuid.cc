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

#include <cassert>
#include <cerrno>
#include <cstring>

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <combaseapi.h>
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
  static_assert(sizeof(GUID) == sizeof(data), "GUID size mismatch");
  GUID guid;
  HRESULT result = CoCreateGuid(&guid);
  FIREBASE_ASSERT(result == S_OK);
  (void)result;
  memcpy(data, &guid, sizeof(data));
#else
  memset(data, 0, sizeof(data));
#ifdef O_CLOEXEC
  int file = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
#else
  int file = open("/dev/urandom", O_RDONLY);
#endif
  FIREBASE_ASSERT(file >= 0);
  if (file < 0) {
    return;
  }
  size_t total_read = 0;
  while (total_read < sizeof(data)) {
    ssize_t n = read(file, data + total_read, sizeof(data) - total_read);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (n == 0) {
      break;
    }
    total_read += static_cast<size_t>(n);
  }
  close(file);
  FIREBASE_ASSERT(total_read == sizeof(data));
  // Set RFC 4122 version 4 and variant bits.
  data[6] = static_cast<uint8_t>((data[6] & 0x0f) | 0x40);
  data[8] = static_cast<uint8_t>((data[8] & 0x3f) | 0x80);
#endif  // FIREBASE_PLATFORM_WINDOWS
}

}  // namespace internal
}  // namespace firebase
