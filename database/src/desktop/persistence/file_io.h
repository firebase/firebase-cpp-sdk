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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_FILE_IO_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_FILE_IO_H_

#include <string>

namespace firebase {
namespace database {
namespace internal {

class FileIoInterface {
 public:
  virtual ~FileIoInterface() = 0;
  virtual bool ClearFile(const char* name) = 0;
  virtual bool AppendToFile(const char* name, const char* buffer,
                            size_t size) = 0;
  virtual bool ReadFromFile(const char* name, std::string* buffer) = 0;
  virtual bool SetByte(const char* name, size_t offset, uint8_t byte) = 0;
};

class FileIo : public FileIoInterface {
 public:
  ~FileIo() override;
  bool ClearFile(const char* name) override;
  bool AppendToFile(const char* name, const char* buffer, size_t size) override;
  bool ReadFromFile(const char* name, std::string* buffer) override;
  bool SetByte(const char* name, size_t offset, uint8_t byte) override;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_FILE_IO_H_
