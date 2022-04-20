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

// This test file does not compile on Android because Android uses the built-in
// sqlite database, not LevelDb.
#if defined(__ANDROID__)
#error "This test should not be included in Android."
#endif

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "Firestore/core/src/local/leveldb_util.h"
#include "Firestore/core/src/util/filesystem.h"
#include "Firestore/core/src/util/path.h"

#include "firebase_test_framework.h"
#include "gtest/gtest.h"
#include "leveldb/db.h"

// TODO(dconeybe) Reference this test in the iOS SDK instead of making a
// copy of it here in the C++ SDK.

namespace {

using ::firebase::firestore::local::ConvertStatus;
using ::firebase::firestore::util::Filesystem;
using ::firebase::firestore::util::Path;

// Creates a LevelDb database that uses Snappy compression for at least one of
// its blocks. Attempting to iterate over this database using a LevelDb library
// that does not have Snappy compression compiled in will return a failed status
// with reason "corruption".
Path CreateLevelDbDatabaseThatUsesSnappyCompression();

// This test ensures that we don't accidentally regress having added in Snappy
// compression support (https://github.com/firebase/firebase-ios-sdk/pull/9596).
TEST(LevelDbSnappy, LevelDbHasSnappySupportCompiledIn) {
  // Do not run this test on iOS because LevelDb in iOS does not support Snappy.
  SKIP_TEST_ON_IOS;

  Path leveldb_path = CreateLevelDbDatabaseThatUsesSnappyCompression();
  if (HasFatalFailure()) return;

  leveldb::Options options;
  options.create_if_missing = false;

  std::unique_ptr<leveldb::DB> db;
  {
    leveldb::DB* db_ptr;
    leveldb::Status status =
        leveldb::DB::Open(options, leveldb_path.ToUtf8String(), &db_ptr);
    ASSERT_TRUE(status.ok());
    db.reset(db_ptr);
  }

  // One of the assertions below will fail when LevelDb attempts to read a block
  // that is compressed with Snappy and Snappy compression support is not
  // compiled in.
  std::unique_ptr<leveldb::Iterator> it(
      db->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    ASSERT_TRUE(it->status().ok()) << ConvertStatus(it->status());
  }
  ASSERT_TRUE(it->status().ok()) << ConvertStatus(it->status());
}

const std::array<unsigned char, 0x00000165> LevelDbSnappyFile_000005_ldb{
    0x84, 0x03, 0x80, 0x00, 0x42, 0x00, 0x85, 0x71, 0x75, 0x65, 0x72, 0x79,
    0x5F, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x01, 0x8B, 0x43, 0x6F,
    0x6C, 0x41, 0x2F, 0x44, 0x6F, 0x63, 0x41, 0x2F, 0x43, 0x6F, 0x6C, 0x42,
    0x01, 0x0A, 0x68, 0x42, 0x7C, 0x66, 0x3A, 0x7C, 0x6F, 0x62, 0x3A, 0x5F,
    0x5F, 0x6E, 0x61, 0x6D, 0x65, 0x5F, 0x5F, 0x61, 0x73, 0x63, 0x00, 0x01,
    0x8C, 0x82, 0x80, 0x01, 0x07, 0x00, 0x05, 0x01, 0x08, 0x01, 0x13, 0x50,
    0x11, 0x3E, 0x01, 0x16, 0x00, 0x0A, 0x05, 0x15, 0xF0, 0x3C, 0x00, 0x08,
    0x02, 0x20, 0x05, 0x32, 0x4A, 0x12, 0x48, 0x70, 0x72, 0x6F, 0x6A, 0x65,
    0x63, 0x74, 0x73, 0x2F, 0x54, 0x65, 0x73, 0x74, 0x54, 0x65, 0x72, 0x6D,
    0x69, 0x6E, 0x61, 0x74, 0x65, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61,
    0x73, 0x65, 0x73, 0x2F, 0x28, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74,
    0x29, 0x2F, 0x64, 0x6F, 0x63, 0x75, 0x6D, 0x65, 0x6E, 0x74, 0x73, 0x01,
    0x7B, 0x3E, 0x85, 0x00, 0x0C, 0x0D, 0x07, 0x50, 0x08, 0x15, 0x5A, 0x00,
    0x03, 0xFE, 0x5A, 0x00, 0x2E, 0x5A, 0x00, 0x38, 0x07, 0x12, 0x06, 0x5F,
    0x67, 0x6C, 0x6F, 0x62, 0x61, 0x6C, 0x00, 0x01, 0x80, 0x01, 0x0B, 0x11,
    0x65, 0x1C, 0x10, 0x05, 0x20, 0x01, 0x12, 0x07, 0x06, 0x09, 0x15, 0x10,
    0x00, 0x03, 0x01, 0x10, 0x04, 0x00, 0x01, 0x09, 0x10, 0x24, 0x01, 0x12,
    0x01, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x01, 0x35, 0x00, 0x06,
    0x09, 0x15, 0x10, 0x37, 0x0C, 0x07, 0x01, 0x05, 0x09, 0x0B, 0x10, 0x36,
    0x0C, 0x07, 0x01, 0x04, 0x09, 0x0B, 0x10, 0x35, 0x0C, 0x07, 0x01, 0x03,
    0x09, 0x0B, 0x4C, 0x34, 0x0C, 0x07, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x2C, 0x6E, 0xE0, 0xF4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0xC0, 0xF2, 0xA1, 0xB0, 0x00, 0x09, 0x03, 0x86, 0x01, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x87, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x58, 0xC2, 0x94, 0x06, 0x8C, 0x02, 0x08,
    0x99, 0x02, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x57, 0xFB, 0x80, 0x8B, 0x24, 0x75, 0x47, 0xDB,
};
const std::array<unsigned char, 0x000000E8> LevelDbSnappyFile_000017_ldb{
    0x00, 0x14, 0x50, 0x85, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x01,
    0x8C, 0x82, 0x80, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
    0x02, 0x20, 0x0A, 0x32, 0x4A, 0x12, 0x48, 0x70, 0x72, 0x6F, 0x6A, 0x65,
    0x63, 0x74, 0x73, 0x2F, 0x54, 0x65, 0x73, 0x74, 0x54, 0x65, 0x72, 0x6D,
    0x69, 0x6E, 0x61, 0x74, 0x65, 0x2F, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61,
    0x73, 0x65, 0x73, 0x2F, 0x28, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74,
    0x29, 0x2F, 0x64, 0x6F, 0x63, 0x75, 0x6D, 0x65, 0x6E, 0x74, 0x73, 0x2F,
    0x43, 0x6F, 0x6C, 0x41, 0x2F, 0x44, 0x6F, 0x63, 0x41, 0x2F, 0x43, 0x6F,
    0x6C, 0x42, 0x2F, 0x44, 0x6F, 0x63, 0x42, 0x07, 0x12, 0x06, 0x5F, 0x67,
    0x6C, 0x6F, 0x62, 0x61, 0x6C, 0x00, 0x01, 0x80, 0x01, 0x0D, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x02, 0x10, 0x0A, 0x20, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xCD, 0xE0, 0x39, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xF2, 0xA1, 0xB0,
    0x00, 0x09, 0x03, 0x86, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x8A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0xE4, 0xA7, 0x7E, 0x74, 0x8F, 0x01, 0x08, 0x9C, 0x01, 0x17, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0xFB, 0x80, 0x8B,
    0x24, 0x75, 0x47, 0xDB,
};
const std::array<unsigned char, 0x00000000> LevelDbSnappyFile_000085_ldb{};
const std::array<unsigned char, 0x00000010> LevelDbSnappyFile_CURRENT{
    0x4D, 0x41, 0x4E, 0x49, 0x46, 0x45, 0x53, 0x54,
    0x2D, 0x30, 0x30, 0x30, 0x30, 0x38, 0x34, 0x0A,
};
const std::array<unsigned char, 0x000000B5> LevelDbSnappyFile_LOG_old{
    0x32, 0x30, 0x32, 0x32, 0x2F, 0x30, 0x34, 0x2F, 0x30, 0x34, 0x2D, 0x31,
    0x31, 0x3A, 0x33, 0x39, 0x3A, 0x34, 0x36, 0x2E, 0x32, 0x35, 0x37, 0x32,
    0x35, 0x31, 0x20, 0x30, 0x78, 0x37, 0x30, 0x30, 0x30, 0x30, 0x35, 0x33,
    0x31, 0x34, 0x30, 0x30, 0x30, 0x20, 0x52, 0x65, 0x63, 0x6F, 0x76, 0x65,
    0x72, 0x69, 0x6E, 0x67, 0x20, 0x6C, 0x6F, 0x67, 0x20, 0x23, 0x38, 0x31,
    0x0A, 0x32, 0x30, 0x32, 0x32, 0x2F, 0x30, 0x34, 0x2F, 0x30, 0x34, 0x2D,
    0x31, 0x31, 0x3A, 0x33, 0x39, 0x3A, 0x34, 0x36, 0x2E, 0x33, 0x30, 0x34,
    0x35, 0x35, 0x32, 0x20, 0x30, 0x78, 0x37, 0x30, 0x30, 0x30, 0x30, 0x35,
    0x33, 0x31, 0x34, 0x30, 0x30, 0x30, 0x20, 0x44, 0x65, 0x6C, 0x65, 0x74,
    0x65, 0x20, 0x74, 0x79, 0x70, 0x65, 0x3D, 0x33, 0x20, 0x23, 0x38, 0x30,
    0x0A, 0x32, 0x30, 0x32, 0x32, 0x2F, 0x30, 0x34, 0x2F, 0x30, 0x34, 0x2D,
    0x31, 0x31, 0x3A, 0x33, 0x39, 0x3A, 0x34, 0x36, 0x2E, 0x33, 0x30, 0x35,
    0x30, 0x36, 0x34, 0x20, 0x30, 0x78, 0x37, 0x30, 0x30, 0x30, 0x30, 0x35,
    0x33, 0x31, 0x34, 0x30, 0x30, 0x30, 0x20, 0x44, 0x65, 0x6C, 0x65, 0x74,
    0x65, 0x20, 0x74, 0x79, 0x70, 0x65, 0x3D, 0x30, 0x20, 0x23, 0x38, 0x31,
    0x0A,
};
const std::array<unsigned char, 0x000000B5> LevelDbSnappyFile_LOG{
    0x32, 0x30, 0x32, 0x32, 0x2F, 0x30, 0x34, 0x2F, 0x30, 0x34, 0x2D, 0x31,
    0x31, 0x3A, 0x35, 0x36, 0x3A, 0x35, 0x36, 0x2E, 0x34, 0x39, 0x33, 0x31,
    0x34, 0x32, 0x20, 0x30, 0x78, 0x37, 0x30, 0x30, 0x30, 0x30, 0x61, 0x32,
    0x35, 0x34, 0x30, 0x30, 0x30, 0x20, 0x52, 0x65, 0x63, 0x6F, 0x76, 0x65,
    0x72, 0x69, 0x6E, 0x67, 0x20, 0x6C, 0x6F, 0x67, 0x20, 0x23, 0x38, 0x33,
    0x0A, 0x32, 0x30, 0x32, 0x32, 0x2F, 0x30, 0x34, 0x2F, 0x30, 0x34, 0x2D,
    0x31, 0x31, 0x3A, 0x35, 0x36, 0x3A, 0x35, 0x36, 0x2E, 0x35, 0x33, 0x34,
    0x37, 0x34, 0x35, 0x20, 0x30, 0x78, 0x37, 0x30, 0x30, 0x30, 0x30, 0x61,
    0x32, 0x35, 0x34, 0x30, 0x30, 0x30, 0x20, 0x44, 0x65, 0x6C, 0x65, 0x74,
    0x65, 0x20, 0x74, 0x79, 0x70, 0x65, 0x3D, 0x33, 0x20, 0x23, 0x38, 0x32,
    0x0A, 0x32, 0x30, 0x32, 0x32, 0x2F, 0x30, 0x34, 0x2F, 0x30, 0x34, 0x2D,
    0x31, 0x31, 0x3A, 0x35, 0x36, 0x3A, 0x35, 0x36, 0x2E, 0x35, 0x33, 0x35,
    0x32, 0x34, 0x32, 0x20, 0x30, 0x78, 0x37, 0x30, 0x30, 0x30, 0x30, 0x61,
    0x32, 0x35, 0x34, 0x30, 0x30, 0x30, 0x20, 0x44, 0x65, 0x6C, 0x65, 0x74,
    0x65, 0x20, 0x74, 0x79, 0x70, 0x65, 0x3D, 0x30, 0x20, 0x23, 0x38, 0x33,
    0x0A,
};
const std::array<unsigned char, 0x000000C2> LevelDbSnappyFile_MANIFEST_000084{
    0x45, 0x63, 0x9F, 0xDD, 0xAC, 0x00, 0x01, 0x01, 0x1A, 0x6C, 0x65, 0x76,
    0x65, 0x6C, 0x64, 0x62, 0x2E, 0x42, 0x79, 0x74, 0x65, 0x77, 0x69, 0x73,
    0x65, 0x43, 0x6F, 0x6D, 0x70, 0x61, 0x72, 0x61, 0x74, 0x6F, 0x72, 0x07,
    0x00, 0x05, 0xE5, 0x02, 0x42, 0x85, 0x71, 0x75, 0x65, 0x72, 0x79, 0x5F,
    0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x01, 0x8B, 0x43, 0x6F, 0x6C,
    0x41, 0x2F, 0x44, 0x6F, 0x63, 0x41, 0x2F, 0x43, 0x6F, 0x6C, 0x42, 0x2F,
    0x44, 0x6F, 0x63, 0x42, 0x7C, 0x66, 0x3A, 0x7C, 0x6F, 0x62, 0x3A, 0x5F,
    0x5F, 0x6E, 0x61, 0x6D, 0x65, 0x5F, 0x5F, 0x61, 0x73, 0x63, 0x00, 0x01,
    0x8C, 0x82, 0x80, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13,
    0x85, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x00, 0x01, 0x80, 0x01,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x11, 0xE8, 0x01,
    0x14, 0x85, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x01, 0x8C, 0x82,
    0x80, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x85, 0x74,
    0x61, 0x72, 0x67, 0x65, 0x74, 0x5F, 0x67, 0x6C, 0x6F, 0x62, 0x61, 0x6C,
    0x00, 0x01, 0x80, 0x01, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB1,
    0x03, 0xAC, 0xBA, 0x08, 0x00, 0x01, 0x02, 0x55, 0x09, 0x00, 0x03, 0x56,
    0x04, 0x0D,
};

template <typename T>
void WriteFile(const Path& dir,
               const std::string& file_name,
               const T& data_array) {
  Filesystem* fs = Filesystem::Default();
  {
    auto status = fs->RecursivelyCreateDir(dir);
    if (!status.ok()) {
      FAIL() << "Creating directory failed: " << dir.ToUtf8String() << " ("
             << status.error_message() << ")";
    }
  }

  Path file = dir.AppendUtf8(file_name);
  std::ofstream out_file(file.native_value(), std::ios::binary);
  if (!out_file) {
    FAIL() << "Unable to open file for writing: " << file.ToUtf8String();
  }

  out_file.write(reinterpret_cast<const char*>(data_array.data()),
                 data_array.size());
  out_file.close();
  if (!out_file) {
    FAIL() << "Writing to file failed: " << file.ToUtf8String();
  }
}

Path LevelDbDir() {
  Filesystem* fs = Filesystem::Default();
  Path dir = fs->TempDir().AppendUtf8("PersistenceTesting");

  // Delete the directory first to ensure isolation between runs.
  auto status = fs->RecursivelyRemove(dir);
  EXPECT_TRUE(status.ok()) << "Failed to clean up leveldb in dir "
                           << dir.ToUtf8String() << ": " << status.ToString();

  return dir;
}

Path CreateLevelDbDatabaseThatUsesSnappyCompression() {
  Path leveldb_dir = LevelDbDir();
  WriteFile(leveldb_dir, "000005.ldb", LevelDbSnappyFile_000005_ldb);
  WriteFile(leveldb_dir, "000017.ldb", LevelDbSnappyFile_000017_ldb);
  WriteFile(leveldb_dir, "000085.ldb", LevelDbSnappyFile_000085_ldb);
  WriteFile(leveldb_dir, "CURRENT", LevelDbSnappyFile_CURRENT);
  WriteFile(leveldb_dir, "LOG.old", LevelDbSnappyFile_LOG_old);
  WriteFile(leveldb_dir, "LOG", LevelDbSnappyFile_LOG);
  WriteFile(leveldb_dir, "MANIFEST-000084", LevelDbSnappyFile_MANIFEST_000084);
  return leveldb_dir;
}

}  // namespace
