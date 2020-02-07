// Copyright 2019 Google LLC
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

#include "database/src/desktop/persistence/file_io.h"

#include <fstream>
#include <iostream>

#include "flatbuffers/util.h"

namespace firebase {
namespace database {
namespace internal {

FileIoInterface::~FileIoInterface() {}

FileIo::~FileIo() {}

bool FileIo::ClearFile(const char* name) {
  std::ofstream ofs(name, std::ios::binary);
  if (!ofs.is_open()) return false;
  ofs.write(nullptr, 0);
  return !ofs.bad();
}

bool FileIo::AppendToFile(const char* name, const char* buffer, size_t size) {
  std::ofstream ofs(name, std::ios::binary | std::ios::app);
  if (!ofs.is_open()) return false;
  ofs.write(buffer, size);
  return !ofs.bad();
}

bool FileIo::ReadFromFile(const char* name, std::string* buffer) {
  bool result = flatbuffers::LoadFile(name, true, buffer);
  return result;
}

bool FileIo::SetByte(const char* name, size_t offset, uint8_t byte) {
  std::fstream ofs(name, std::ios::binary | std::ios::in | std::ios::out);
  ofs.seekp(offset);
  ofs.put(byte);
  return !ofs.bad();
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
