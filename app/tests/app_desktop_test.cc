/*
 * Copyright 2023 Google LLC
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

#include "app/src/app_desktop.h"

#include <stdio.h>

#include <codecvt>
#include <cstdio>
#include <fstream>
#include <string>

#include "app/src/app_common.h"
#include "app/src/app_identifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace firebase {

static bool RemoveDirectoryUtf8(std::string dir_utf8) {
#if FIREBASE_PLATFORM_WINDOWS
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> cvt;
  std::wstring dir_utf16 = cvt.from_bytes(dir_utf8);
  return (_wrmdir(dir_utf16.c_str()) == 0);
#else
  return (rmdir(dir_utf8.c_str()) == 0);
#endif
}

static bool MakeDirectoryUtf8(std::string dir_utf8) {
#if FIREBASE_PLATFORM_WINDOWS
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> cvt;
  std::wstring dir_utf16 = cvt.from_bytes(dir_utf8);
  return (_wmkdir(dir_utf16.c_str()) == 0);
#else
  return (mkdir(dir_utf8.c_str(), 0700) == 0);
#endif
}

static bool ChangeDirectoryUtf8(std::string dir_utf8) {
#if FIREBASE_PLATFORM_WINDOWS
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> cvt;
  std::wstring dir_utf16 = cvt.from_bytes(dir_utf8);
  return (_wchdir(dir_utf16.c_str()) == 0);
#else
  return (chdir(dir_utf8.c_str()) == 0);
#endif
}

static std::string GetCurrentDirectoryUtf8() {
#if FIREBASE_PLATFORM_WINDOWS
  wchar_t path_buffer[FILENAME_MAX];
  _wgetcwd(path_buffer, FILENAME_MAX);
  std::string path_utf8;
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> cvt;
  path_utf8 = cvt.to_bytes(path_buffer);
  return path_utf8;
#else
  char path_buffer[FILENAME_MAX];
  getcwd(path_buffer, FILENAME_MAX);
  return std::string(path_buffer);
#endif
}

TEST(AppDesktopTest, TestSetAppId) {
  AppOptions options;
  options.set_app_id("abc");
  EXPECT_STREQ("abc", options.app_id());
}

TEST(AppDesktopTest, TestSetApiKey) {
  AppOptions options;
  options.set_api_key("AIzaSyDdVgKwhZl0sTTTLZ7iTmt1r3N2cJLnaDk");
  EXPECT_STREQ("AIzaSyDdVgKwhZl0sTTTLZ7iTmt1r3N2cJLnaDk", options.api_key());
}

TEST(AppDesktopTest, TestSetMessagingSenderId) {
  AppOptions options;
  options.set_messaging_sender_id("012345678901");
  EXPECT_STREQ("012345678901", options.messaging_sender_id());
}

TEST(AppDesktopTest, TestSetDatabaseUrl) {
  AppOptions options;
  options.set_database_url("http://abc-xyz-123.firebaseio.com");
  EXPECT_STREQ("http://abc-xyz-123.firebaseio.com", options.database_url());
}

TEST(AppDesktopTest, TestSetGaTrackingId) {
  AppOptions options;
  options.set_ga_tracking_id("UA-12345678-1");
  EXPECT_STREQ("UA-12345678-1", options.ga_tracking_id());
}

TEST(AppDesktopTest, TestSetStorageBucket) {
  AppOptions options;
  options.set_storage_bucket("abc-xyz-123.storage.firebase.com");
  EXPECT_STREQ("abc-xyz-123.storage.firebase.com", options.storage_bucket());
}

TEST(AppDesktopTest, TestSetProjectId) {
  AppOptions options;
  options.set_project_id("myproject-123");
  EXPECT_STREQ("myproject-123", options.project_id());
}

static const char kGoogleServicesJsonContent[] = R"""(
{
  "client": [
    {
      "services": {
        "appinvite_service": {
          "status": 1
        },
        "analytics_service": {
          "status": 0
        }
      },
      "oauth_client": [
        {
          "client_id": "fake client id"
        }
      ],
      "api_key": [
        {
          "current_key": "fake api key"
        }
      ],
      "client_info": {
        "mobilesdk_app_id": "fake app id",
        "android_client_info": {
          "package_name": "com.testproject.packagename"
        }
      }
    }
  ],
  "configuration_version": "1",
  "project_info": {
    "storage_bucket": "fake storage bucket",
    "project_id": "fake project id",
    "firebase_url": "fake database url",
    "project_number": "fake messaging sender id"
  }
}
)""";

namespace internal {
// Defined in app_desktop.cc
bool LoadAppOptionsFromJsonConfigFile(const char* path, AppOptions* options);
}  // namespace internal

TEST(AppDesktopTest, TestLoadAppOptionsFromJsonConfigFile) {
  // Write out a config file and then load it from a full path.
#if FIREBASE_PLATFORM_WINDOWS
  std::wofstream outfile(L"fake-google-services-1.json");
  const char kPathSep[] = "\\";
#else
  std::ofstream outfile("fake-google-services-1.json");
  const char kPathSep[] = "/";
#endif  // FIREBASE_PLATFORM_WINDOWS

  outfile << kGoogleServicesJsonContent;
  outfile.close();

  std::string json_path =
      GetCurrentDirectoryUtf8() + kPathSep + "fake-google-services-1.json";

  LogInfo("JSON path: %s", json_path.c_str());

  AppOptions options;
  EXPECT_TRUE(
      internal::LoadAppOptionsFromJsonConfigFile(json_path.c_str(), &options));

  EXPECT_STREQ("fake app id", options.app_id());
  EXPECT_STREQ("fake api key", options.api_key());
  EXPECT_STREQ("fake project id", options.project_id());

#if FIREBASE_PLATFORM_WINDOWS
  EXPECT_EQ(_wremove(L"fake-google-services-1.json"), 0);
#else
  EXPECT_EQ(remove("fake-google-services-1.json"), 0);
#endif  // FIREBASE_PLATFORM_WINDOWS
}

TEST(AppDesktopTest, TestLoadAppOptionsFromJsonConfigFileInInternationalPath) {
  // Save the current directory.
  std::string previous_directory = GetCurrentDirectoryUtf8();
  EXPECT_NE(previous_directory, "");

  // Make a new directory, with international characters.
  std::string new_dir = "téŝt_dir";
  if (!ChangeDirectoryUtf8(new_dir)) {
    // If the target directory doesn't exist, make it.
    LogInfo("Creating directory: %s", new_dir.c_str());
    ASSERT_TRUE(MakeDirectoryUtf8(new_dir));
    ASSERT_TRUE(ChangeDirectoryUtf8(new_dir));
  }
  // Write out a config file and then load it from a full path with
  // international characters.
#if FIREBASE_PLATFORM_WINDOWS
  const char kPathSep[] = "\\";
  std::wofstream outfile(L"fake-google-services-2.json");
#else
  const char kPathSep[] = "/";
  std::ofstream outfile("fake-google-services-2.json");
#endif  // FIREBASE_PLATFORM_WINDOWS

  outfile << kGoogleServicesJsonContent;
  outfile.close();

  std::string json_path =
      GetCurrentDirectoryUtf8() + kPathSep + "fake-google-services-2.json";

  LogInfo("JSON path: %s", json_path.c_str());

  AppOptions options;
  EXPECT_TRUE(
      internal::LoadAppOptionsFromJsonConfigFile(json_path.c_str(), &options));

  EXPECT_STREQ("fake app id", options.app_id());
  EXPECT_STREQ("fake api key", options.api_key());
  EXPECT_STREQ("fake project id", options.project_id());

#if FIREBASE_PLATFORM_WINDOWS
  EXPECT_EQ(_wremove(L"fake-google-services-2.json"), 0);
#else
  EXPECT_EQ(remove("fake-google-services-2.json"), 0);
#endif  // FIREBASE_PLATFORM_WINDOWS

  EXPECT_TRUE(ChangeDirectoryUtf8(previous_directory));
  EXPECT_TRUE(RemoveDirectoryUtf8(new_dir));
}
}  // namespace firebase
