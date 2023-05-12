// Copyright 2019 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <inttypes.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>  // NOLINT

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/auth.h"
#include "firebase/internal/platform.h"
#include "firebase/storage.h"
#include "firebase/util.h"
#include "firebase_test_framework.h"  // NOLINT

// The TO_STRING macro is useful for command line defined strings as the quotes
// get stripped.
#define TO_STRING_EXPAND(X) #X
#define TO_STRING(X) TO_STRING_EXPAND(X)

// Path to the Firebase config file to load.
#ifdef FIREBASE_CONFIG
#define FIREBASE_CONFIG_STRING TO_STRING(FIREBASE_CONFIG)
#else
#define FIREBASE_CONFIG_STRING ""
#endif  // FIREBASE_CONFIG

// Allow integration tests to enable retrying regardless of error type.
#if FIREBASE_PLATFORM_DESKTOP
namespace firebase {
namespace storage {
namespace internal {
extern bool g_retry_all_errors_for_testing;
}  // namespace internal
}  // namespace storage
}  // namespace firebase
#endif

namespace firebase_testapp_automated {

using app_framework::LogDebug;
using app_framework::LogError;

// You can customize the Storage URL here.
const char* kStorageUrl = nullptr;

#if FIREBASE_PLATFORM_DESKTOP
// Use a larger file on desktop.
const int kLargeFileMegabytes = 64;
#elif FIREBASE_PLATFORM_ANDROID
// Android FTL devices can sometimes be really slow.
// Use a much smaller file.
const int kLargeFileMegabytes = 8;
#else  // iOS or tvOS
// iOS FTL devices can handle a medium-sized file.
const int kLargeFileMegabytes = 32;
#endif

const char kRootNodeName[] = "integration_test_data";

using app_framework::GetCurrentTimeInMicroseconds;
using app_framework::PathForResource;
using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;
using testing::ElementsAreArray;

class FirebaseStorageTest : public FirebaseTest {
 public:
  FirebaseStorageTest();
  ~FirebaseStorageTest() override;

  // Called once before all tests.
  static void SetUpTestSuite();
  // Called once after all tests.
  static void TearDownTestSuite();

  // Called at the start of each test.
  void SetUp() override;
  // Called after each test.
  void TearDown() override;

  // File references that we need to delete on test exit.
 protected:
  // Initialize Firebase App and Firebase Auth.
  static void InitializeAppAndAuth();
  // Shut down Firebase App and Firebase Auth.
  static void TerminateAppAndAuth();

  // Sign in an anonymous user.
  static void SignIn();
  // Sign out the current user, if applicable.
  // If this is an anonymous user, deletes the user instead,
  // to avoid polluting the user list.
  static void SignOut();

  // Initialize Firebase Storage.
  void InitializeStorage();
  // Shut down Firebase Storage.
  void TerminateStorage();

  // Create a unique working folder and return a reference to it.
  firebase::storage::StorageReference CreateFolder();

  static firebase::App* shared_app_;
  static firebase::auth::Auth* shared_auth_;

  bool initialized_;
  firebase::storage::Storage* storage_;
  // File references that we need to delete on test exit.
  std::vector<firebase::storage::StorageReference> cleanup_files_;
  std::string saved_url_;
};
// Initialization flow looks like this:
//  - Once, before any tests run:
//  -   SetUpTestSuite: Initialize App and Auth. Sign in.
//  - For each test:
//    - SetUp: Initialize Storage.
//    - Run the test.
//    - TearDown: Shut down Storage.
//  - Once, after all tests are finished:
//  -   TearDownTestSuite: Sign out. Shut down Auth and App.

firebase::App* FirebaseStorageTest::shared_app_;
firebase::auth::Auth* FirebaseStorageTest::shared_auth_;

FirebaseStorageTest::FirebaseStorageTest()
    : initialized_(false), storage_(nullptr) {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseStorageTest::~FirebaseStorageTest() {
  // Must be cleaned up on exit.
  assert(storage_ == nullptr);
}

void FirebaseStorageTest::SetUpTestSuite() { InitializeAppAndAuth(); }

void FirebaseStorageTest::InitializeAppAndAuth() {
  LogDebug("Initialize Firebase App.");

  FindFirebaseConfig(FIREBASE_CONFIG_STRING);

#if defined(__ANDROID__)
  shared_app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                        app_framework::GetActivity());
#else
  shared_app_ = ::firebase::App::Create();
#endif  // defined(__ANDROID__)

  ASSERT_NE(shared_app_, nullptr);

  LogDebug("Initializing Auth.");

  // Initialize Firebase Auth.
  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(shared_app_, &shared_auth_,
                         [](::firebase::App* app, void* target) {
                           LogDebug("Attempting to initialize Firebase Auth.");
                           ::firebase::InitResult result;
                           *reinterpret_cast<firebase::auth::Auth**>(target) =
                               ::firebase::auth::Auth::GetAuth(app, &result);
                           return result;
                         });

  WaitForCompletion(initializer.InitializeLastResult(), "InitializeAuth");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Auth.");

  ASSERT_NE(shared_auth_, nullptr);

  // Sign in anonymously.
  SignIn();
}

void FirebaseStorageTest::TearDownTestSuite() { TerminateAppAndAuth(); }

void FirebaseStorageTest::TerminateAppAndAuth() {
  if (shared_auth_) {
    LogDebug("Signing out.");
    SignOut();
    LogDebug("Shutdown Auth.");
    delete shared_auth_;
    shared_auth_ = nullptr;
  }
  if (shared_app_) {
    LogDebug("Shutdown App.");
    delete shared_app_;
    shared_app_ = nullptr;
  }
}

void FirebaseStorageTest::SetUp() {
  FirebaseTest::SetUp();
  InitializeStorage();
}

void FirebaseStorageTest::TearDown() {
  if (initialized_) {
    if (!cleanup_files_.empty() && storage_ && shared_app_) {
      LogDebug("Cleaning up files.");
      std::vector<firebase::Future<void>> cleanups;
      cleanups.reserve(cleanup_files_.size());
      for (int i = 0; i < cleanup_files_.size(); ++i) {
        cleanups.push_back(cleanup_files_[i].Delete());
      }
      for (int i = 0; i < cleanups.size(); ++i) {
        WaitForCompletionAnyResult(cleanups[i],
                                   "FirebaseStorageTest::TearDown");
      }
      cleanup_files_.clear();
    }
  }
  TerminateStorage();
  FirebaseTest::TearDown();
}

void FirebaseStorageTest::InitializeStorage() {
  LogDebug("Initializing Firebase Storage.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(
      shared_app_, &storage_, [](::firebase::App* app, void* target) {
        LogDebug("Attempting to initialize Firebase Storage.");
        ::firebase::InitResult result;
        *reinterpret_cast<firebase::storage::Storage**>(target) =
            firebase::storage::Storage::GetInstance(app, kStorageUrl, &result);
        return result;
      });

  WaitForCompletion(initializer.InitializeLastResult(), "InitializeStorage");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Storage.");

  initialized_ = true;
}

void FirebaseStorageTest::TerminateStorage() {
  if (!initialized_) return;

  if (storage_) {
    LogDebug("Shutdown the Storage library.");
    delete storage_;
    storage_ = nullptr;
  }

  initialized_ = false;

  ProcessEvents(100);
}

void FirebaseStorageTest::SignIn() {
  if (shared_auth_->current_user().is_valid()) {
    // Already signed in.
    return;
  }
  LogDebug("Signing in.");
  firebase::Future<firebase::auth::AuthResult> sign_in_future =
      shared_auth_->SignInAnonymously();
  WaitForCompletion(sign_in_future, "SignInAnonymously");
  if (sign_in_future.error() != 0) {
    FAIL() << "Ensure your application has the Anonymous sign-in provider "
              "enabled in Firebase Console.";
  }
  ProcessEvents(100);
}

void FirebaseStorageTest::SignOut() {
  if (shared_auth_ == nullptr) {
    // Auth is not set up.
    return;
  }
  if (!shared_auth_->current_user().is_valid()) {
    // Already signed out.
    return;
  }
  if (shared_auth_->current_user().is_anonymous()) {
    // If signed in anonymously, delete the anonymous user.
    WaitForCompletion(shared_auth_->current_user().Delete(),
                      "DeleteAnonymousUser");
  } else {
    // If not signed in anonymously (e.g. if the tests were modified to sign in
    // as an actual user), just sign out normally.
    shared_auth_->SignOut();

    // Wait for the sign-out to finish.
    while (shared_auth_->current_user().is_valid()) {
      if (ProcessEvents(100)) break;
    }
  }
  EXPECT_FALSE(shared_auth_->current_user().is_valid());
}

firebase::storage::StorageReference FirebaseStorageTest::CreateFolder() {
  // Generate a folder for the test data based on the time in milliseconds.
  int64_t time_in_microseconds = GetCurrentTimeInMicroseconds();

  char buffer[21] = {0};
  snprintf(buffer, sizeof(buffer), "%lld",
           static_cast<long long>(time_in_microseconds));  // NOLINT
  saved_url_ = buffer;
  return storage_->GetReference(kRootNodeName).Child(saved_url_);
}

// Test cases below.

TEST_F(FirebaseStorageTest, TestInitializeAndTerminate) {
  // Already tested via SetUp() and TearDown().
}

TEST_F(FirebaseStorageTest, TestSignIn) {
  EXPECT_TRUE(shared_auth_->current_user().is_valid());
}

TEST_F(FirebaseStorageTest, TestCreateWorkingFolder) {
  SignIn();
  // Create a unique child in the storage that we can run our tests in.
  firebase::storage::StorageReference ref = CreateFolder();
  EXPECT_NE(saved_url_, "");

  LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
           ref.full_path().c_str());
  // Create the same reference in a few different manners and ensure they're
  // equivalent.
  // NOLINTNEXTLINE intentional redundant string conversion
  {
    firebase::storage::StorageReference ref_from_str =
        storage_->GetReference(std::string(kRootNodeName)).Child(saved_url_);
    EXPECT_EQ(ref.bucket(), ref_from_str.bucket());
    EXPECT_EQ(ref.full_path(), ref_from_str.full_path());
  }
  std::string url = "gs://" + ref.bucket() + "/" + kRootNodeName;
  LogDebug("Calling GetReferenceFromUrl(%s)", url.c_str());
  firebase::storage::StorageReference ref_from_url =
      storage_->GetReferenceFromUrl(url.c_str()).Child(saved_url_);
  EXPECT_TRUE(ref_from_url.is_valid());
  EXPECT_EQ(ref.bucket(), ref_from_url.bucket());
  EXPECT_EQ(ref.full_path(), ref_from_url.full_path());
  firebase::storage::StorageReference ref_from_url_str =
      storage_->GetReferenceFromUrl(url).Child(saved_url_);
  EXPECT_TRUE(ref_from_url_str.is_valid());
  EXPECT_EQ(ref.bucket(), ref_from_url_str.bucket());
  EXPECT_EQ(ref.full_path(), ref_from_url_str.full_path());
}

TEST_F(FirebaseStorageTest, TestStorageUrl) {
  SignIn();
  // Confirm that creating a storage instance with a URL returns a url(), and
  // creating a storage instance with a null URL returns a blank url().
  std::string default_url =
      std::string("gs://") + storage_->GetReference().bucket();

  // Check whether the Storage instance we already have is handled correctly.
  EXPECT_EQ(storage_->url(), kStorageUrl ? kStorageUrl : "");
  delete storage_;
  storage_ = nullptr;
  {
    firebase::storage::Storage* storage_explicit =
        firebase::storage::Storage::GetInstance(shared_app_,
                                                default_url.c_str(), nullptr);
    ASSERT_NE(storage_explicit, nullptr);
    EXPECT_EQ(storage_explicit->url(), default_url);
    delete storage_explicit;
  }
  {
    firebase::storage::Storage* storage_implicit =
        firebase::storage::Storage::GetInstance(shared_app_, nullptr, nullptr);
    ASSERT_NE(storage_implicit, nullptr);
    EXPECT_EQ(storage_implicit->url(), "");
    delete storage_implicit;
  }
}

// NOLINTNEXTLINE
const std::string kSimpleTestFile =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
    "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
    "aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum.";

TEST_F(FirebaseStorageTest, TestWriteAndReadByteBuffer) {
  SKIP_TEST_ON_ANDROID_EMULATOR;

  SignIn();

  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile.txt");
  LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
           ref.full_path().c_str());
  cleanup_files_.push_back(ref);
  // Write to a simple file.
  {
    LogDebug("Upload sample file from memory.");
    firebase::Future<firebase::storage::Metadata> future =
        RunWithRetry<firebase::storage::Metadata>([&]() {
          return ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size());
        });
    WaitForCompletion(future, "PutBytes");
    auto metadata = future.result();
    EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
  }

  // Now read back the file.
  {
    LogDebug("Download sample file to memory.");
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    memset(buffer, 0, sizeof(buffer));

    firebase::Future<size_t> future = RunWithRetry<size_t>(
        [&]() { return ref.GetBytes(buffer, kBufferSize); });
    WaitForCompletion(future, "GetBytes");
    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kSimpleTestFile.size());
    EXPECT_THAT(kSimpleTestFile, ElementsAreArray(buffer, file_size))
        << "Download failed, file contents did not match.";
  }
}

TEST_F(FirebaseStorageTest, TestWriteAndReadFileWithCustomMetadata) {
  SignIn();

  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-CustomMetadata.txt");
  LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
           ref.full_path().c_str());
  cleanup_files_.push_back(ref);
  std::string content_type = "text/plain";
  std::string custom_metadata_key = "specialkey";
  std::string custom_metadata_value = "secret value";
  // Write to a simple file.
  {
    LogDebug("Write a sample file with custom metadata from byte buffer.");
    firebase::storage::Metadata metadata;
    metadata.set_content_type(content_type.c_str());
    (*metadata.custom_metadata())[custom_metadata_key] = custom_metadata_value;
    firebase::Future<firebase::storage::Metadata> future =
        ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size(), metadata);
    WaitForCompletion(future, "PutBytes");
    const firebase::storage::Metadata* metadata_written = future.result();
    ASSERT_NE(metadata_written, nullptr);
    EXPECT_EQ(metadata_written->size_bytes(), kSimpleTestFile.size());
    EXPECT_EQ(metadata_written->content_type(), content_type);
    auto& custom_metadata = *metadata_written->custom_metadata();
    EXPECT_EQ(custom_metadata[custom_metadata_key], custom_metadata_value);
  }
  // Now read back the file.
  {
    LogDebug("Download sample file with custom metadata to memory.");
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    memset(buffer, 0, sizeof(buffer));

    firebase::Future<size_t> future = RunWithRetry<size_t>(
        [&]() { return ref.GetBytes(buffer, kBufferSize); });
    WaitForCompletion(future, "GetBytes");
    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kSimpleTestFile.size());
    EXPECT_THAT(kSimpleTestFile, ElementsAreArray(buffer, file_size))
        << "Download failed, file contents did not match.";
  }
  // And read the custom metadata.
  {
    LogDebug("Read custom metadata.");
    firebase::Future<firebase::storage::Metadata> future =
        RunWithRetry<firebase::storage::Metadata>(
            [&]() { return ref.GetMetadata(); });
    WaitForCompletion(future, "GetFileMetadata");
    const firebase::storage::Metadata* metadata = future.result();
    ASSERT_NE(metadata, nullptr);

    // Get the current time to compare to the Timestamp.
    int64_t current_time_seconds = static_cast<int64_t>(time(nullptr));
    int64_t updated_time_milliseconds = metadata->updated_time();
    int64_t updated_time_seconds = updated_time_milliseconds / 1000;
    int64_t time_difference_seconds =
        updated_time_seconds - current_time_seconds;
    // As long as our timestamp is within a day, it's correct enough for
    // our purposes.
    const int64_t kAllowedTimeDifferenceSeconds = 60L * 60L * 24L;
    EXPECT_TRUE(time_difference_seconds < kAllowedTimeDifferenceSeconds &&
                time_difference_seconds > -kAllowedTimeDifferenceSeconds)
        << "Bad timestamp in metadata.";
    EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    EXPECT_EQ(metadata->content_type(), content_type);
    ASSERT_NE(metadata->custom_metadata(), nullptr);
    auto& custom_metadata = *metadata->custom_metadata();
    EXPECT_EQ(custom_metadata[custom_metadata_key], custom_metadata_value);
  }
}

// 1x1 transparent PNG file
static const unsigned char kEmptyPngFileBytes[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
    0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xfc, 0xcf, 0xc0, 0x50,
    0x0f, 0x00, 0x04, 0x85, 0x01, 0x80, 0x84, 0xa9, 0x8c, 0x21, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

TEST_F(FirebaseStorageTest, TestWriteAndReadCustomContentType) {
  SignIn();

  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-CustomContentType.png");
  LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
           ref.full_path().c_str());
  cleanup_files_.push_back(ref);
  std::string content_type = "image/png";
  // Write to a simple file.
  {
    LogDebug("Write a sample file with custom content-type from byte buffer.");
    firebase::storage::Metadata metadata;
    metadata.set_content_type(content_type.c_str());
    firebase::Future<firebase::storage::Metadata> future =
        ref.PutBytes(kEmptyPngFileBytes, sizeof(kEmptyPngFileBytes), metadata);
    WaitForCompletion(future, "PutBytes");
    const firebase::storage::Metadata* metadata_written = future.result();
    ASSERT_NE(metadata_written, nullptr);
    EXPECT_EQ(metadata_written->content_type(), content_type);
  }
  // Now read back the file.
  {
    LogDebug("Download sample file with custom content-type to memory.");
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    memset(buffer, 0, sizeof(buffer));

    firebase::Future<size_t> future = RunWithRetry<size_t>(
        [&]() { return ref.GetBytes(buffer, kBufferSize); });
    WaitForCompletion(future, "GetBytes");
    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, sizeof(kEmptyPngFileBytes));
    EXPECT_THAT(kEmptyPngFileBytes, ElementsAreArray(buffer, file_size))
        << "Download failed, file contents did not match.";
  }
  // And read the custom content type
  {
    LogDebug("Read custom content-type.");
    firebase::Future<firebase::storage::Metadata> future =
        RunWithRetry<firebase::storage::Metadata>(
            [&]() { return ref.GetMetadata(); });
    WaitForCompletion(future, "GetFileMetadata");
    const firebase::storage::Metadata* metadata = future.result();
    ASSERT_NE(metadata, nullptr);

    EXPECT_EQ(metadata->content_type(), content_type);
  }
}

const char kPutFileTestFile[] = "PutFileTest.txt";
const char kGetFileTestFile[] = "GetFileTest.txt";
const char kFileUriScheme[] = "file://";

TEST_F(FirebaseStorageTest, TestPutFileAndGetFile) {
  SignIn();

  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-FileIO.txt");
  cleanup_files_.push_back(ref);

  // Upload a file.
  {
    // Write file that we're going to upload.
    std::string path = PathForResource() + kPutFileTestFile;
    // Cloud Storage expects a URI, so add file:// in front of local
    // paths.
    std::string file_path = kFileUriScheme + path;

    LogDebug("Creating local file: %s", path.c_str());

    FILE* file = fopen(path.c_str(), "wb");
    size_t bytes_written =
        std::fwrite(kSimpleTestFile.c_str(), 1, kSimpleTestFile.size(), file);
    EXPECT_EQ(bytes_written, kSimpleTestFile.size());
    fclose(file);

    firebase::storage::Metadata new_metadata;
    std::string content_type = "text/plain";
    new_metadata.set_content_type(content_type);

    LogDebug("Uploading sample file from disk.");
    firebase::Future<firebase::storage::Metadata> future =
        RunWithRetry<firebase::storage::Metadata>(
            [&]() { return ref.PutFile(file_path.c_str(), new_metadata); });
    WaitForCompletion(future, "PutFile");
    EXPECT_NE(future.result(), nullptr);
    const firebase::storage::Metadata* metadata = future.result();
#if !TARGET_OS_IPHONE && !TARGET_OS_TV
    // Disable this specific check on iOS/tvOS, due to a possible race condition
    // in the Storage iOS library. TODO(b/230800306): Revisit this later.
    EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
#endif  // !TARGET_OS_IPHONE && !TARGET_OS_TV
    EXPECT_EQ(metadata->content_type(), content_type);
  }
  // Use GetBytes to ensure the file uploaded correctly.
  {
    LogDebug("Downloading file to disk.");
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    memset(buffer, 0, sizeof(buffer));

    firebase::Future<size_t> future = RunWithRetry<size_t>(
        [&]() { return ref.GetBytes(buffer, kBufferSize); });
    WaitForCompletion(future, "GetBytes");
    EXPECT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kSimpleTestFile.size());
    EXPECT_EQ(memcmp(&kSimpleTestFile[0], &buffer[0], file_size), 0);
  }
  // Test GetFile to ensure we can download to a file.
  {
    std::string path = PathForResource() + kGetFileTestFile;
    // Cloud Storage expects a URI, so add file:// in front of local
    // paths.
    std::string file_path = kFileUriScheme + path;

    LogDebug("Saving to local file: %s", path.c_str());

    firebase::Future<size_t> future =
        RunWithRetry<size_t>([&]() { return ref.GetFile(file_path.c_str()); });
    WaitForCompletion(future, "GetFile");
    EXPECT_NE(future.result(), nullptr);
    EXPECT_EQ(*future.result(), kSimpleTestFile.size());

    std::vector<char> buffer(kSimpleTestFile.size());
    FILE* file = fopen(path.c_str(), "rb");
    EXPECT_NE(file, nullptr);
    size_t bytes_read = std::fread(&buffer[0], 1, kSimpleTestFile.size(), file);
    EXPECT_EQ(bytes_read, kSimpleTestFile.size());
    fclose(file);
    EXPECT_EQ(memcmp(&kSimpleTestFile[0], &buffer[0], buffer.size()), 0);
  }
}

TEST_F(FirebaseStorageTest, TestDownloadUrl) {
  SignIn();

  const char kTestFileName[] = "TestFile-DownloadUrl.txt";
  firebase::storage::StorageReference ref = CreateFolder().Child(kTestFileName);
  cleanup_files_.push_back(ref);

  LogDebug("Uploading file.");
  WaitForCompletion(RunWithRetry([&]() {
                      return ref.PutBytes(&kSimpleTestFile[0],
                                          kSimpleTestFile.size());
                    }),
                    "PutBytes");

  LogDebug("Getting download URL.");
  firebase::Future<std::string> future =
      RunWithRetry<std::string>([&]() { return ref.GetDownloadUrl(); });
  WaitForCompletion(future, "GetDownloadUrl");
  ASSERT_NE(future.result(), nullptr);
  LogDebug("Got download URL: %s", future.result()->c_str());
  // Check for a somewhat well-formed URL, i.e. it starts with "https://" and
  // has "TestFile-DownloadUrl" in the name.
  EXPECT_TRUE(future.result()->find("https://") == 0)
      << "Download Url doesn't start with https://";
  EXPECT_TRUE(future.result()->find(kTestFileName) != std::string::npos)
      << "Download Url doesn't contain the filename " << kTestFileName;
}

TEST_F(FirebaseStorageTest, TestDeleteFile) {
  SignIn();

  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-Delete.txt");
  // Don't add to cleanup_files_ because we are going to delete it anyway.

  LogDebug("Uploading file.");
  WaitForCompletion(RunWithRetry([&]() {
                      return ref.PutBytes(&kSimpleTestFile[0],
                                          kSimpleTestFile.size());
                    }),
                    "PutBytes");

  LogDebug("Deleting file.");
  WaitForCompletion(ref.Delete(), "Delete");

  // Need a placeholder buffer.
  const size_t kBufferSize = 1024;
  char buffer[kBufferSize];
  // Ensure the file was deleted.
  LogDebug("Ensuring file was deleted.");
  firebase::Future<size_t> future = ref.GetBytes(buffer, kBufferSize);
  WaitForCompletion(future, "GetBytes",
                    firebase::storage::kErrorObjectNotFound);
}

// Only test retries on desktop since Android and iOS don't have an option
// to retry file-not-found errors and just pass-through to native
// implementations.
#if FIREBASE_PLATFORM_DESKTOP
TEST_F(FirebaseStorageTest, TestGetBytesWithMaxRetryDuration) {
  // Enable retrying of file-not-found errors for testing.
  bool old_value = firebase::storage::internal::g_retry_all_errors_for_testing;
  firebase::storage::internal::g_retry_all_errors_for_testing = true;

  int shorter_duration_sec = 2;
  int longer_duration_sec = 6;
  SignIn();

  // Call GetBytes on a non-existent ref. Call PutBytes while the GetBytes is
  // still retrying. Verify that GetBytes succeeds.
  {
    LogDebug("Call PutBytes while GetBytes is retrying.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File2.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&] {
      // PutBytes after a short delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * shorter_duration_sec));
      LogDebug("Upload sample file from memory.");
      firebase::Future<firebase::storage::Metadata> future =
          RunWithRetry<firebase::storage::Metadata>([&]() {
            return ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size());
          });
      WaitForCompletion(future, "PutBytes");
      auto metadata = future.result();
      EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    });
    // GetBytes with a long retry duration.
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    memset(buffer, 0, sizeof(buffer));
    storage_->set_max_download_retry_time(longer_duration_sec);
    firebase::Future<size_t> future = ref.GetBytes(buffer, kBufferSize);
    WaitForCompletion(future, "GetBytes");
    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kSimpleTestFile.size());
    EXPECT_THAT(kSimpleTestFile, ElementsAreArray(buffer, file_size))
        << "Download failed, file contents did not match.";

    t1.join();
  }

  // Call GetBytes on a non-existent ref. Call PutBytes after GetBytes should
  // have stopped retrying. Verify that GetBytes fails.
  {
    LogDebug("Call PutBytes after the maximum retry deadline.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File3.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&] {
      // PutBytes after a long delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * longer_duration_sec));
      LogDebug("Upload sample file from memory.");
      firebase::Future<firebase::storage::Metadata> future =
          RunWithRetry<firebase::storage::Metadata>([&]() {
            return ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size());
          });
      WaitForCompletion(future, "PutBytes");
      auto metadata = future.result();
      EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    });
    // GetBytes with a short retry duration
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    LogDebug("Ensuring file does not exist.");
    storage_->set_max_download_retry_time(shorter_duration_sec);
    firebase::Future<size_t> future = ref.GetBytes(buffer, kBufferSize);
    WaitForCompletion(future, "GetBytes",
                      firebase::storage::kErrorObjectNotFound);
    t1.join();
  }

  firebase::storage::internal::g_retry_all_errors_for_testing = old_value;
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(FirebaseStorageTest, TestGetMetadataWithMaxRetryDuration) {
  // Enable retrying of file-not-found errors for testing.
  bool old_value = firebase::storage::internal::g_retry_all_errors_for_testing;
  firebase::storage::internal::g_retry_all_errors_for_testing = true;

  int shorter_duration_sec = 2;
  int longer_duration_sec = 6;
  SignIn();

  std::string content_type = "text/plain";
  std::string custom_metadata_key = "specialkey";
  std::string custom_metadata_value = "secret value";
  // Call GetMetadata on a non-existent ref. Call PutBytes while the GetMetadata
  // is still retrying. Verify that GetMetadata succeeds.
  {
    LogDebug("Call PutBytes while GetMetadata is retrying.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File2.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&] {
      // PutBytes after a short delay with custom metadata.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * shorter_duration_sec));
      firebase::storage::Metadata metadata;
      metadata.set_content_type(content_type.c_str());
      (*metadata.custom_metadata())[custom_metadata_key] =
          custom_metadata_value;
      LogDebug("Upload sample file from memory.");
      firebase::Future<firebase::storage::Metadata> future =
          RunWithRetry<firebase::storage::Metadata>([&]() {
            return ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size(),
                                metadata);
          });
      WaitForCompletion(future, "PutBytes");
      auto response_metadata = future.result();
      EXPECT_EQ(response_metadata->size_bytes(), kSimpleTestFile.size());
    });
    // GetMetadata with a long retry duration
    storage_->set_max_operation_retry_time(longer_duration_sec);
    LogDebug("Read custom metadata.");
    firebase::Future<firebase::storage::Metadata> future = ref.GetMetadata();
    WaitForCompletion(future, "GetFileMetadata");
    const firebase::storage::Metadata* metadata = future.result();
    ASSERT_NE(metadata, nullptr);
    EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    EXPECT_EQ(metadata->content_type(), content_type);
    ASSERT_NE(metadata->custom_metadata(), nullptr);
    auto& custom_metadata = *metadata->custom_metadata();
    EXPECT_EQ(custom_metadata[custom_metadata_key], custom_metadata_value);
    t1.join();
  }

  // Call GetMetadata on a non-existent ref. Call PutBytes after GetMetadata
  // should have stopped retrying. Verify that GetMetadata fails.
  {
    LogDebug("Call PutBytes after the maximum retry deadline.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File3.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&] {
      // PutBytes after a long delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * longer_duration_sec));
      LogDebug("Upload sample file from memory.");
      firebase::Future<firebase::storage::Metadata> future =
          RunWithRetry<firebase::storage::Metadata>([&]() {
            return ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size());
          });
      WaitForCompletion(future, "PutBytes");
      auto metadata = future.result();
      EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    });
    // GetMetadata with a short retry duration
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    LogDebug("Ensuring GetMetadata fails because file does not exist.");
    storage_->set_max_operation_retry_time(shorter_duration_sec);
    LogDebug("Read custom metadata.");
    firebase::Future<firebase::storage::Metadata> future = ref.GetMetadata();
    WaitForCompletion(future, "GetFileMetadata",
                      firebase::storage::kErrorObjectNotFound);
    t1.join();
  }

  firebase::storage::internal::g_retry_all_errors_for_testing = old_value;
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(FirebaseStorageTest, TestGetFileWithMaxRetryDuration) {
  // Enable retrying of file-not-found errors for testing.
  bool old_value = firebase::storage::internal::g_retry_all_errors_for_testing;
  firebase::storage::internal::g_retry_all_errors_for_testing = true;

  int shorter_duration_sec = 2;
  int longer_duration_sec = 6;
  SignIn();

  // Call GetFile on a non-existent ref. Call PutFile while the GetFile is
  // still retrying. Verify that GetFile succeeds.
  {
    LogDebug("Call PutFile while GetFile is retrying.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File2.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&] {
      // PufFile after a short delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * shorter_duration_sec));
      // Write file that we're going to upload.
      std::string path = PathForResource() + kPutFileTestFile;
      std::string file_path = kFileUriScheme + path;
      LogDebug("Creating local file: %s", path.c_str());
      FILE* file = fopen(path.c_str(), "wb");
      std::fwrite(kSimpleTestFile.c_str(), 1, kSimpleTestFile.size(), file);
      fclose(file);
      firebase::storage::Metadata new_metadata;
      std::string content_type = "text/plain";
      new_metadata.set_content_type(content_type);

      LogDebug("Uploading sample file from disk.");
      firebase::Future<firebase::storage::Metadata> future =
          RunWithRetry<firebase::storage::Metadata>(
              [&]() { return ref.PutFile(file_path.c_str(), new_metadata); });
      WaitForCompletion(future, "PutFile");
      EXPECT_NE(future.result(), nullptr);
      const firebase::storage::Metadata* metadata = future.result();
      EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
      EXPECT_EQ(metadata->content_type(), content_type);
    });

    // GetFile with a long retry duration.
    storage_->set_max_download_retry_time(longer_duration_sec);
    std::string path = PathForResource() + kGetFileTestFile;
    // Cloud Storage expects a URI, so add file:// in front of local
    // paths.
    std::string file_path = kFileUriScheme + path;
    LogDebug("Saving to local file: %s", path.c_str());
    firebase::Future<size_t> future =
        RunWithRetry<size_t>([&]() { return ref.GetFile(file_path.c_str()); });
    WaitForCompletion(future, "GetFile");
    EXPECT_NE(future.result(), nullptr);
    EXPECT_EQ(*future.result(), kSimpleTestFile.size());
    std::vector<char> buffer(kSimpleTestFile.size());
    FILE* file = fopen(path.c_str(), "rb");
    EXPECT_NE(file, nullptr);
    std::fread(&buffer[0], 1, kSimpleTestFile.size(), file);
    fclose(file);
    EXPECT_EQ(memcmp(&kSimpleTestFile[0], &buffer[0], buffer.size()), 0);

    t1.join();
  }

  // Call GetFile on a non-existent ref. Call PutFile after GetFile should
  // have stopped retrying. Verify that GetFile fails.
  {
    LogDebug("Call PutFile after the maximum retry deadline.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File2.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&] {
      // PufFile after a long delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * longer_duration_sec));
      // Write file that we're going to upload.
      std::string path = PathForResource() + kPutFileTestFile;
      std::string file_path = kFileUriScheme + path;
      LogDebug("Creating local file: %s", path.c_str());
      FILE* file = fopen(path.c_str(), "wb");
      std::fwrite(kSimpleTestFile.c_str(), 1, kSimpleTestFile.size(), file);
      fclose(file);
      firebase::storage::Metadata new_metadata;
      std::string content_type = "text/plain";
      new_metadata.set_content_type(content_type);

      LogDebug("Uploading sample file from disk.");
      firebase::Future<firebase::storage::Metadata> future =
          RunWithRetry<firebase::storage::Metadata>(
              [&]() { return ref.PutFile(file_path.c_str(), new_metadata); });
      WaitForCompletion(future, "PutFile");
      EXPECT_NE(future.result(), nullptr);
      const firebase::storage::Metadata* metadata = future.result();
      EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
      EXPECT_EQ(metadata->content_type(), content_type);
    });

    // GetFile with a short retry duration.
    storage_->set_max_download_retry_time(shorter_duration_sec);
    std::string path = PathForResource() + kGetFileTestFile;
    // Cloud Storage expects a URI, so add file:// in front of local
    // paths.
    std::string file_path = kFileUriScheme + path;
    LogDebug("Ensuring file does not exist.");
    firebase::Future<size_t> future = ref.GetFile(file_path.c_str());
    WaitForCompletion(future, "GetFile",
                      firebase::storage::kErrorObjectNotFound);

    t1.join();
  }

  firebase::storage::internal::g_retry_all_errors_for_testing = old_value;
}
#endif  // FIREBASE_PLATFORM_DESKTOP

// Only test retries on desktop since Android and iOS don't have an option
// to retry file-not-found errors and just pass-through to native
// implementations.
#if FIREBASE_PLATFORM_DESKTOP
TEST_F(FirebaseStorageTest, TestDeleteWithMaxRetryDuration) {
  // Enable retrying of file-not-found errors for testing.
  bool old_value = firebase::storage::internal::g_retry_all_errors_for_testing;
  firebase::storage::internal::g_retry_all_errors_for_testing = true;

  int shorter_duration_sec = 2;
  int longer_duration_sec = 6;
  SignIn();

  // Call Delete on a non-existent ref. Call PutBytes while the Delete is
  // still retrying. Verify that Delete succeeds.
  {
    LogDebug("Call PutBytes while Delete is retrying.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File2.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&] {
      // PutBytes after a short delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * shorter_duration_sec));
      LogDebug("Upload sample file from memory.");
      firebase::Future<firebase::storage::Metadata> future =
          RunWithRetry<firebase::storage::Metadata>([&]() {
            return ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size());
          });
      WaitForCompletion(future, "PutBytes");
      auto metadata = future.result();
      EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    });
    // Call Delete with a long retry duration.
    storage_->set_max_operation_retry_time(longer_duration_sec);
    LogDebug("Deleting file.");
    WaitForCompletion(ref.Delete(), "Delete");

    // Join the thread that called PutBytes and verify that file was deleted.
    t1.join();
    // Need a placeholder buffer.
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    // Ensure the file was deleted.
    LogDebug("Ensuring file was deleted.");
    // Disable retrying of file-not-found errors while verifying that file does
    // not exist.
    firebase::storage::internal::g_retry_all_errors_for_testing = false;
    firebase::Future<size_t> future = ref.GetBytes(buffer, kBufferSize);
    WaitForCompletion(future, "GetBytes",
                      firebase::storage::kErrorObjectNotFound);
    firebase::storage::internal::g_retry_all_errors_for_testing = true;
  }

  // Call Delete on a non-existent ref. Call PutBytes after Delete should
  // have stopped retrying. Verify that Delete fails.
  {
    LogDebug("Call PutBytes after the maximum retry deadline.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File3.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&] {
      // PutBytes after a long delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * longer_duration_sec));
      LogDebug("Upload sample file from memory.");
      firebase::Future<firebase::storage::Metadata> future =
          RunWithRetry<firebase::storage::Metadata>([&]() {
            return ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size());
          });
      WaitForCompletion(future, "PutBytes");
      auto metadata = future.result();
      EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    });

    // Call Delete with a short retry duration.
    storage_->set_max_operation_retry_time(shorter_duration_sec);
    LogDebug("Deleting file.");
    WaitForCompletion(ref.Delete(), "Delete",
                      firebase::storage::kErrorObjectNotFound);
    t1.join();
  }

  firebase::storage::internal::g_retry_all_errors_for_testing = old_value;
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(FirebaseStorageTest, TestPutFileWithMaxRetryDuration) {
  // Enable retrying of future errors for testing. This will retry putfile when
  // it is unable to read the local file.
  bool old_value = firebase::storage::internal::g_retry_all_errors_for_testing;
  firebase::storage::internal::g_retry_all_errors_for_testing = true;

  int shorter_duration_sec = 2;
  int longer_duration_sec = 6;
  SignIn();

  std::string path = PathForResource() + kPutFileTestFile;
  // Cloud Storage expects a URI, so add file:// in front of local
  // paths.
  std::string file_path = kFileUriScheme + path;

  // Delete the local file first to guarantee it does not already exist.
  std::remove(path.c_str());

  // Call PutFile on a non-existent local file. Create the file while PutFile is
  // still retrying. Verify that PutFile succeeds.
  {
    LogDebug("Create local file while PutFile is retrying.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File2.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&, path] {
      // Write local file after a short delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * shorter_duration_sec));
      // Write file that we're going to upload.
      LogDebug("Creating local file: %s", path.c_str());
      FILE* file = fopen(path.c_str(), "wb");
      std::fwrite(kSimpleTestFile.c_str(), 1, kSimpleTestFile.size(), file);
      fclose(file);
    });

    // PutFile with a long retry duration.
    storage_->set_max_upload_retry_time(longer_duration_sec);
    firebase::storage::Metadata new_metadata;
    std::string content_type = "text/plain";
    new_metadata.set_content_type(content_type);

    LogDebug("Uploading sample file from disk.");
    firebase::Future<firebase::storage::Metadata> future =
        ref.PutFile(file_path.c_str(), new_metadata);
    WaitForCompletion(future, "PutFile");
    EXPECT_NE(future.result(), nullptr);
    const firebase::storage::Metadata* metadata = future.result();
    EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    EXPECT_EQ(metadata->content_type(), content_type);
    t1.join();
  }

  // Delete the local file first to guarantee it does not already exist.
  std::remove(path.c_str());

  // Call PutFile on a non-existent local file. Create the file after PutFile
  // should have stopped retrying. Verify that PutFile succeeds.
  {
    LogDebug("Create local file after the maximum retry deadline.");
    firebase::storage::StorageReference ref = CreateFolder().Child("File2.txt");
    LogDebug("Storage URL: gs://%s%s", ref.bucket().c_str(),
             ref.full_path().c_str());
    cleanup_files_.push_back(ref);

    std::thread t1([&, path] {
      // Write local file after a short delay.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 * longer_duration_sec));
      // Write file that we're going to upload.
      LogDebug("Creating local file: %s", path.c_str());
      FILE* file = fopen(path.c_str(), "wb");
      std::fwrite(kSimpleTestFile.c_str(), 1, kSimpleTestFile.size(), file);
      fclose(file);
    });

    // PutFile with a long retry duration.
    storage_->set_max_upload_retry_time(shorter_duration_sec);
    firebase::storage::Metadata new_metadata;
    std::string content_type = "text/plain";
    new_metadata.set_content_type(content_type);

    LogDebug("Uploading sample file from disk.");
    firebase::Future<firebase::storage::Metadata> future =
        ref.PutFile(file_path.c_str(), new_metadata);
    WaitForCompletion(future, "PutFile", firebase::storage::kErrorUnknown);
    t1.join();
  }

  firebase::storage::internal::g_retry_all_errors_for_testing = old_value;
}
#endif  // FIREBASE_PLATFORM_DESKTOP

class StorageListener : public firebase::storage::Listener {
 public:
  StorageListener()
      : on_paused_was_called_(false),
        on_progress_was_called_(false),
        resume_succeeded_(false),
        last_bytes_transferred_(-1) {}

  // Tracks whether OnPaused was ever called and resumes the transfer.
  void OnPaused(firebase::storage::Controller* controller) override {
#if FIREBASE_PLATFORM_DESKTOP
    // Let things be paused for a moment on desktop, since it typically has a
    // very fast connection.
    ProcessEvents(1000);
#endif  // FIREBASE_PLATFORM_DESKTOP
    on_paused_was_called_ = true;
    LogDebug("Resuming");
    resume_succeeded_ = FirebaseTest::RunFlakyBlock(
        [](firebase::storage::Controller* controller_) {
          return controller_->Resume();
        },
        controller, "Resume");
    if (resume_succeeded_) {
      LogDebug("Resume succeeded");
    }
  }

  void OnProgress(firebase::storage::Controller* controller) override {
    int64_t bytes_transferred = controller->bytes_transferred();
    // Only update when the byte count changed, to avoid spamming the log.
    if (last_bytes_transferred_ != bytes_transferred) {
      LogDebug("Transferred %lld of %lld", bytes_transferred,
               controller->total_byte_count());
      last_bytes_transferred_ = bytes_transferred;
    }
    on_progress_was_called_ = true;
  }

  bool on_paused_was_called() const { return on_paused_was_called_; }
  bool on_progress_was_called() const { return on_progress_was_called_; }
  bool resume_succeeded() const { return resume_succeeded_; }

 public:
  bool on_paused_was_called_;
  bool on_progress_was_called_;
  bool resume_succeeded_;
  int64_t last_bytes_transferred_;
};

// Contents of a large file, "X" will be replaced with a different character
// each line.
const char kLargeFileString[] =
    "X: This is a large file with multiple lines and even some \xB1nary "
    "char\xAC\ters.\n";

std::string CreateDataForLargeFile(size_t size_bytes) {
  std::string large_test_file;
  const std::string kLine = kLargeFileString;

  large_test_file.reserve(size_bytes + kLine.size());
  char replacement[2] = "a";
  while (large_test_file.size() < size_bytes) {
    std::string line = kLine;
    line.replace(kLine.find("X"), 1, replacement);
    large_test_file += line;
    replacement[0] = (replacement[0] - 'a' + 1) % 26 + 'a';
  }
  large_test_file.resize(size_bytes);
  return large_test_file;
}

TEST_F(FirebaseStorageTest, TestLargeFilePauseResumeAndDownloadCancel) {
  SignIn();

  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-LargeFile.txt");
  cleanup_files_.push_back(ref);

  const size_t kLargeFileSize = kLargeFileMegabytes * 1024 * 1024;
  const std::string kLargeTestFile = CreateDataForLargeFile(kLargeFileSize);

  FLAKY_TEST_SECTION_BEGIN();

  LogDebug("Uploading large file with pause/resume.");
  StorageListener listener;
  firebase::storage::Controller controller;
  firebase::Future<firebase::storage::Metadata> future = ref.PutBytes(
      kLargeTestFile.c_str(), kLargeFileSize, &listener, &controller);

  // Ensure the Controller is valid now that we have associated it
  // with an operation.
  EXPECT_TRUE(controller.is_valid());

  while (controller.bytes_transferred() == 0) {
#if FIREBASE_PLATFORM_DESKTOP
    ProcessEvents(1);
#else  // FIREBASE_PLATFORM_MOBILE
    ProcessEvents(500);
#endif
  }

  // After waiting a moment for the operation to start (above), pause
  // the operation and verify it was successfully paused when the
  // future completes.
  LogDebug("Pausing upload.");
  if (!FirebaseTest::RunFlakyBlock(
          [](firebase::storage::Controller* controller) {
            return controller->Pause();
          },
          &controller, "Pause")) {
    FAIL() << "Pause failed.";
  }

  // The StorageListener's OnPaused will call Resume().

  LogDebug("Waiting for future.");
  WaitForCompletion(future, "WriteLargeFile");
  LogDebug("Upload complete.");

  // Ensure the various callbacks were called.
  EXPECT_TRUE(listener.on_paused_was_called());
  EXPECT_TRUE(listener.on_progress_was_called());
  EXPECT_TRUE(listener.resume_succeeded());

  auto metadata = future.result();
  // If metadata reports incorrect size, file failed to upload.
  EXPECT_EQ(metadata->size_bytes(), kLargeFileSize);

  FLAKY_TEST_SECTION_END();

  // Download the file and confirm it's correct.
  {
    std::vector<char> buffer(kLargeFileSize);
    memset(&buffer[0], 0, kLargeFileSize);
    LogDebug("Downloading large file for comparison.");
    StorageListener listener;
    firebase::Future<size_t> future = RunWithRetry<size_t>(
        [&]() { return ref.GetBytes(&buffer[0], kLargeFileSize, &listener); });
    WaitForCompletion(future, "GetBytes");
    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kLargeFileSize) << "Read size did not match";
    EXPECT_TRUE(memcmp(kLargeTestFile.c_str(), &buffer[0], kLargeFileSize) == 0)
        << "Read large file failed, contents did not match.";
  }
#if FIREBASE_PLATFORM_DESKTOP
  FLAKY_TEST_SECTION_BEGIN();

  // Test pausing/resuming while downloading (desktop only).
  std::vector<char> buffer(kLargeFileSize);
  memset(&buffer[0], 0, kLargeFileSize);
  LogDebug("Downloading large file with pausing/resuming.");
  StorageListener listener;
  firebase::storage::Controller controller;
  firebase::Future<size_t> future =
      ref.GetBytes(&buffer[0], kLargeFileSize, &listener, &controller);
  EXPECT_TRUE(controller.is_valid());

  while (controller.bytes_transferred() == 0) {
    ProcessEvents(1);
  }

  LogDebug("Pausing download.");
  if (!FirebaseTest::RunFlakyBlock(
          [](firebase::storage::Controller* controller) {
            return controller->Pause();
          },
          &controller, "Pause")) {
    FAIL() << "Pause failed";
  }

  WaitForCompletion(future, "GetBytes");

  LogDebug("Download complete.");

  // Ensure the progress and pause callbacks were called.
  EXPECT_TRUE(listener.on_paused_was_called());
  EXPECT_TRUE(listener.on_progress_was_called());
  EXPECT_TRUE(listener.resume_succeeded());
  EXPECT_NE(future.result(), nullptr);
  size_t file_size = *future.result();
  EXPECT_EQ(file_size, kLargeFileSize);
  EXPECT_EQ(memcmp(kLargeTestFile.c_str(), &buffer[0], kLargeFileSize), 0);

  FLAKY_TEST_SECTION_END();
#else
  {
    std::vector<char> buffer(kLargeFileSize);
    // Test downloading large file (mobile only), without pausing, as mobile
    // does not support pause during file download, only upload.
    memset(&buffer[0], 0, kLargeFileSize);
    LogDebug("Downloading large file.");
    StorageListener listener;
    firebase::storage::Controller controller;
    firebase::Future<size_t> future = RunWithRetry<size_t>([&]() {
      return ref.GetBytes(&buffer[0], kLargeFileSize, &listener, &controller);
    });
    ASSERT_TRUE(controller.is_valid());

    WaitForCompletion(future, "GetBytes");
    LogDebug("Download complete.");

    // Ensure the progress callback was called.
    EXPECT_TRUE(listener.on_progress_was_called());
    EXPECT_FALSE(listener.on_paused_was_called());

    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kLargeFileSize) << "Read size did not match";
    EXPECT_TRUE(memcmp(kLargeTestFile.c_str(), &buffer[0], kLargeFileSize) == 0)
        << "Read large file failed, contents did not match.";
  }
#endif  // FIREBASE_PLATFORM_DESKTOP

  // Try canceling while downloading.
  FLAKY_TEST_SECTION_BEGIN();

  std::vector<char> buffer(kLargeFileSize);
  LogDebug("Downloading large file with cancellation.");
  StorageListener listener;
  firebase::storage::Controller controller;
  firebase::Future<size_t> future =
      ref.GetBytes(&buffer[0], kLargeFileSize, &listener, &controller);
  EXPECT_TRUE(controller.is_valid());

  while (controller.bytes_transferred() == 0) {
    ProcessEvents(1);
  }

  LogDebug("Cancelling download.");
  EXPECT_TRUE(controller.Cancel());
#if FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  // TODO(b/255839066): Change this to expect kErrorCancelled once iOS SDK
  // returns the correct error code.
  //
  // iOS/tvOS SDK doesn't always report kErrorCancelled, so ensure that
  // either it was reported as cancelled, or the file was not fully uploaded.
  WaitForCompletionAnyResult(future, "GetBytes");
  EXPECT_TRUE(
      future.error() == firebase::storage::kErrorCancelled ||
      future.error() == firebase::storage::kErrorUnknown ||
      (future.error() == 0 && controller.bytes_transferred() < kLargeFileSize));
#else
  WaitForCompletion(future, "GetBytes", firebase::storage::kErrorCancelled);
#endif

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseStorageTest, TestLargeFileCancelUpload) {
  SignIn();

  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-LargeFileCancel.txt");

  const size_t kLargeFileSize = kLargeFileMegabytes * 1024 * 1024;
  const std::string kLargeTestFile = CreateDataForLargeFile(kLargeFileSize);
  FLAKY_TEST_SECTION_BEGIN();

  LogDebug("Write a large file and cancel mid-way.");
  StorageListener listener;
  firebase::storage::Controller controller;
  firebase::Future<firebase::storage::Metadata> future = ref.PutBytes(
      kLargeTestFile.c_str(), kLargeFileSize, &listener, &controller);

  // Ensure the Controller is valid now that we have associated it
  // with an operation.
  EXPECT_TRUE(controller.is_valid());

  while (controller.bytes_transferred() == 0) {
    ProcessEvents(1);
  }

  LogDebug("Cancelling upload.");
  // Cancel the operation and verify it was successfully canceled.
  EXPECT_TRUE(controller.Cancel());

#if FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  // TODO(b/255839066): Change this to expect kErrorCancelled once iOS SDK
  // returns the correct error code.
  //
  // iOS/tvOS SDK doesn't always report kErrorCancelled, so ensure that
  // either it was reported as cancelled, or the file was not fully uploaded.
  WaitForCompletionAnyResult(future, "PutBytes");
  EXPECT_TRUE(
      future.error() == firebase::storage::kErrorCancelled ||
      future.error() == firebase::storage::kErrorUnknown ||
      (future.error() == 0 && controller.bytes_transferred() < kLargeFileSize));
#else
  WaitForCompletion(future, "PutBytes", firebase::storage::kErrorCancelled);
#endif  // FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseStorageTest, TestInvalidatingReferencesWhenDeletingStorage) {
  SignIn();

  // Create a file so we can get its metadata and check that it's properly
  // invalidated.
  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-InvalidateReferencesDeletingStorage.txt");
  // Don't clean up, will be manually deleted.

  WaitForCompletion(RunWithRetry([&]() {
                      return ref.PutBytes(&kSimpleTestFile[0],
                                          kSimpleTestFile.size());
                    }),
                    "PutBytes");
  ASSERT_NE(ref.PutBytesLastResult().result(), nullptr);
  firebase::storage::Metadata metadata = *ref.PutBytesLastResult().result();
  WaitForCompletion(RunWithRetry([&]() { return ref.Delete(); }), "Delete");

  ASSERT_TRUE(ref.is_valid());
  ASSERT_TRUE(metadata.is_valid());
  delete storage_;
  storage_ = nullptr;
  EXPECT_FALSE(ref.is_valid());
  EXPECT_FALSE(metadata.is_valid());
}

TEST_F(FirebaseStorageTest, TestInvalidatingReferencesWhenDeletingApp) {
  SignIn();

  // Create a file so we can get its metadata and check that it's properly
  // invalidated.
  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-InvalidateReferencesDeletingApp.txt");
  // Don't clean up, will be manually deleted.

  WaitForCompletion(RunWithRetry([&]() {
                      return ref.PutBytes(&kSimpleTestFile[0],
                                          kSimpleTestFile.size());
                    }),
                    "PutBytes");
  ASSERT_NE(ref.PutBytesLastResult().result(), nullptr);
  firebase::storage::Metadata metadata = *ref.PutBytesLastResult().result();
  WaitForCompletion(RunWithRetry([&]() { return ref.Delete(); }), "Delete");

  ASSERT_TRUE(ref.is_valid());
  ASSERT_TRUE(metadata.is_valid());

  delete shared_app_;
  shared_app_ = nullptr;

  EXPECT_FALSE(ref.is_valid());
  EXPECT_FALSE(metadata.is_valid());

  // Fully shut down App and Auth so they can be reinitialized.
  TerminateAppAndAuth();
  // Reinitialize App and Auth.
  InitializeAppAndAuth();
}

}  // namespace firebase_testapp_automated
