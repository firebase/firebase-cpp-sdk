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

namespace firebase_testapp_automated {

using app_framework::LogDebug;

// You can customize the Storage URL here.
const char* kStorageUrl = nullptr;

#if FIREBASE_PLATFORM_DESKTOP
// Use a larger file on desktop...
const int kLargeFileMegabytes = 25;
#else
// ...and a smaller file on mobile.
const int kLargeFileMegabytes = 10;
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

  void SetUp() override;
  void TearDown() override;

 protected:
  // Initialize Firebase App, Firebase Auth, and Firebase Storage.
  void Initialize();
  // Shut down Firebase Storage, Firebase Auth, and Firebase App.
  void Terminate();
  // Sign in an anonymous user.
  void SignIn();
  // Create a unique working folder and return a reference to it.
  firebase::storage::StorageReference CreateFolder();

  bool initialized_;
  firebase::auth::Auth* auth_;
  firebase::storage::Storage* storage_;
  // File references that we need to delete on test exit.
  std::vector<firebase::storage::StorageReference> cleanup_files_;
  std::string saved_url_;
};

FirebaseStorageTest::FirebaseStorageTest()
    : initialized_(false), auth_(nullptr), storage_(nullptr) {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseStorageTest::~FirebaseStorageTest() {
  // Must be cleaned up on exit.
  assert(app_ == nullptr);
  assert(auth_ == nullptr);
  assert(storage_ == nullptr);
}

void FirebaseStorageTest::SetUp() {
  FirebaseTest::SetUp();
  Initialize();
}

void FirebaseStorageTest::TearDown() {
  if (initialized_) {
    Terminate();
  }
  FirebaseTest::TearDown();
}

void FirebaseStorageTest::Initialize() {
  if (initialized_) return;

  InitializeApp();

  LogDebug("Initializing Firebase Auth and Cloud Storage.");

  // 0th element has a reference to this object, the rest have the initializer
  // targets.
  void* initialize_targets[] = {&auth_, &storage_};

  const firebase::ModuleInitializer::InitializerFn initializers[] = {
      [](::firebase::App* app, void* data) {
        void** targets = reinterpret_cast<void**>(data);
        LogDebug("Attempting to initialize Firebase Auth.");
        ::firebase::InitResult result;
        *reinterpret_cast<::firebase::auth::Auth**>(targets[0]) =
            ::firebase::auth::Auth::GetAuth(app, &result);
        return result;
      },
      [](::firebase::App* app, void* data) {
        void** targets = reinterpret_cast<void**>(data);
        LogDebug("Attempting to initialize Cloud Storage.");
        ::firebase::InitResult result;
        firebase::storage::Storage* storage =
            firebase::storage::Storage::GetInstance(app, kStorageUrl, &result);
        *reinterpret_cast<::firebase::storage::Storage**>(targets[1]) = storage;
        return result;
      }};

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(app_, initialize_targets, initializers,
                         sizeof(initializers) / sizeof(initializers[0]));

  WaitForCompletion(initializer.InitializeLastResult(), "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Auth and Cloud Storage.");

  initialized_ = true;
}

void FirebaseStorageTest::Terminate() {
  if (!initialized_) return;

  if (storage_) {
    LogDebug("Cleaning up files.");
    std::vector<firebase::Future<void>> cleanups;
    cleanups.reserve(cleanup_files_.size());
    for (int i = 0; i < cleanup_files_.size(); ++i) {
      cleanups.push_back(cleanup_files_[i].Delete());
    }
    for (int i = 0; i < cleanups.size(); ++i) {
      WaitForCompletion(cleanups[i], "FirebaseStorageTest::TearDown");
    }
    cleanup_files_.clear();

    LogDebug("Shutdown the Storage library.");
    delete storage_;
    storage_ = nullptr;
  }
  if (auth_) {
    LogDebug("Shutdown the Auth library.");
    delete auth_;
    auth_ = nullptr;
  }

  TerminateApp();

  initialized_ = false;

  ProcessEvents(100);
}

void FirebaseStorageTest::SignIn() {
  LogDebug("Signing in.");
  firebase::Future<firebase::auth::User*> sign_in_future =
      auth_->SignInAnonymously();
  WaitForCompletion(sign_in_future, "SignInAnonymously");
  if (sign_in_future.error() != 0) {
    FAIL() << "Ensure your application has the Anonymous sign-in provider "
              "enabled in Firebase Console.";
  }
  ProcessEvents(100);
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
  SignIn();
  EXPECT_NE(auth_->current_user(), nullptr);
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
        firebase::storage::Storage::GetInstance(app_, default_url.c_str(),
                                                nullptr);
    ASSERT_NE(storage_explicit, nullptr);
    EXPECT_EQ(storage_explicit->url(), default_url);
    delete storage_explicit;
  }
  {
    firebase::storage::Storage* storage_implicit =
        firebase::storage::Storage::GetInstance(app_, nullptr, nullptr);
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
        ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size());
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

    firebase::Future<size_t> future = ref.GetBytes(buffer, kBufferSize);
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

    firebase::Future<size_t> future = ref.GetBytes(buffer, kBufferSize);
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
    firebase::Future<firebase::storage::Metadata> future = ref.GetMetadata();
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
    // Cloud Storage expects a URI, so add file:// in front of local paths.
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
        ref.PutFile(file_path.c_str(), new_metadata);
    WaitForCompletion(future, "PutFile");
    ASSERT_NE(future.result(), nullptr);
    const firebase::storage::Metadata* metadata = future.result();
    EXPECT_EQ(metadata->size_bytes(), kSimpleTestFile.size());
    EXPECT_EQ(metadata->content_type(), content_type);
  }

  // Use GetBytes to ensure the file uploaded correctly.
  {
    LogDebug("Downloading file to disk.");
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    memset(buffer, 0, sizeof(buffer));

    firebase::Future<size_t> future = ref.GetBytes(buffer, kBufferSize);
    WaitForCompletion(future, "GetBytes");
    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kSimpleTestFile.size());
    EXPECT_THAT(kSimpleTestFile, ElementsAreArray(buffer, file_size))
        << "Read file to byte buffer failed, file contents did not match.";
  }
  // Test GetFile to ensure we can download to a file.
  {
    std::string path = PathForResource() + kGetFileTestFile;
    // Cloud Storage expects a URI, so add file:// in front of local paths.
    std::string file_path = kFileUriScheme + path;

    LogDebug("Saving to local file: %s", path.c_str());

    firebase::Future<size_t> future = ref.GetFile(file_path.c_str());
    WaitForCompletion(future, "GetFile");
    ASSERT_NE(future.result(), nullptr);
    EXPECT_EQ(*future.result(), kSimpleTestFile.size());

    std::vector<char> buffer(kSimpleTestFile.size());
    FILE* file = fopen(path.c_str(), "rb");
    ASSERT_NE(file, nullptr);
    std::fread(&buffer[0], 1, kSimpleTestFile.size(), file);
    fclose(file);
    EXPECT_THAT(kSimpleTestFile, ElementsAreArray(&buffer[0], buffer.size()))
        << "Download to disk failed, file contents did not match.";
  }
}

TEST_F(FirebaseStorageTest, TestDownloadUrl) {
  SignIn();

  const char kTestFileName[] = "TestFile-DownloadUrl.txt";
  firebase::storage::StorageReference ref = CreateFolder().Child(kTestFileName);
  cleanup_files_.push_back(ref);

  LogDebug("Uploading file.");
  WaitForCompletion(ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size()),
                    "PutBytes");

  LogDebug("Getting download URL.");
  firebase::Future<std::string> future = ref.GetDownloadUrl();
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
  WaitForCompletion(ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size()),
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

class StorageListener : public firebase::storage::Listener {
 public:
  StorageListener()
      : on_paused_was_called_(false), on_progress_was_called_(false) {}

  // Tracks whether OnPaused was ever called and resumes the transfer.
  void OnPaused(firebase::storage::Controller* controller) override {
    LogDebug("Listener OnPaused callback invoked!");
    on_paused_was_called_ = true;
    controller->Resume();
  }

  void OnProgress(firebase::storage::Controller* controller) override {
    LogDebug("Transferred %lld of %lld", controller->bytes_transferred(),
             controller->total_byte_count());
    on_progress_was_called_ = true;
  }

  bool on_paused_was_called() const { return on_paused_was_called_; }
  bool on_progress_was_called() const { return on_progress_was_called_; }

 public:
  bool on_paused_was_called_;
  bool on_progress_was_called_;
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

  {
    LogDebug("Uploading large file with pause/resume.");
    StorageListener listener;
    firebase::storage::Controller controller;
    firebase::Future<firebase::storage::Metadata> future = ref.PutBytes(
        kLargeTestFile.c_str(), kLargeFileSize, &listener, &controller);

    // Ensure the Controller is valid now that we have associated it with an
    // operation.
    ASSERT_TRUE(controller.is_valid());

    // Wait for the transfer to commence.
    while(controller.bytes_transferred() >= 0) {
      ProcessEvents(1);
    }

    // After waiting a moment for the operation to start, pause the operation
    // and verify it was successfully paused. Note that pause might not take
    // effect immediately, so we give it a few moments to pause before
    // failing.
    LogDebug("Pausing upload.");
    EXPECT_TRUE(controller.Pause()) << "Upload pause";

    // allow the callback to be invoked.
    ProcessEvents(1);

    // The StorageListener's OnPaused will call Resume().
    LogDebug("Waiting for future.");
    WaitForCompletion(future, "WriteLargeFile");
    LogDebug("Upload complete.");

    // Ensure the various callbacks were called.
    EXPECT_TRUE(listener.on_paused_was_called());
    EXPECT_TRUE(listener.on_progress_was_called());

    ASSERT_EQ(future.error(), firebase::storage::kErrorNone);
    auto metadata = future.result();
    ASSERT_EQ(metadata->size_bytes(), kLargeFileSize)
        << "Metadata reports incorrect size, file failed to upload.";
  }

  // Download the file and confirm it's correct.
  std::vector<char> buffer(kLargeFileSize);
  {
    memset(&buffer[0], 0, kLargeFileSize);
    LogDebug("Downloading large file for comparison.");
    StorageListener listener;
    firebase::Future<size_t> future =
        ref.GetBytes(&buffer[0], kLargeFileSize, &listener);
    WaitForCompletion(future, "GetBytes");
    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kLargeFileSize) << "Read size did not match";
    EXPECT_TRUE(memcmp(kLargeTestFile.c_str(), &buffer[0], kLargeFileSize) == 0)
        << "Read large file failed, contents did not match.";
  }
#if FIREBASE_PLATFORM_DESKTOP
  {
    // Test pausing/resuming while downloading (desktop only).
    memset(&buffer[0], 0, kLargeFileSize);
    LogDebug("Downloading large file with pausing/resuming.");
    StorageListener listener;
    firebase::storage::Controller controller;
    firebase::Future<size_t> future =
        ref.GetBytes(&buffer[0], kLargeFileSize, &listener, &controller);
    ASSERT_TRUE(controller.is_valid());

    // Wait for the transfer to commence.
    while(controller.bytes_transferred() >= 0) {
      ProcessEvents(1);
    }

    LogDebug("Pausing download.");
    EXPECT_TRUE(controller.Pause()) << "Download pause";

    WaitForCompletion(future, "GetBytes");
    LogDebug("Download complete.");

    // Ensure the progress and pause callbacks were called.
    EXPECT_TRUE(listener.on_progress_was_called());
    EXPECT_TRUE(listener.on_paused_was_called());

    ASSERT_NE(future.result(), nullptr);
    size_t file_size = *future.result();
    EXPECT_EQ(file_size, kLargeFileSize)
        << "Read size with pause/resume did not match";
    EXPECT_TRUE(memcmp(kLargeTestFile.c_str(), &buffer[0], kLargeFileSize) == 0)
        << "Read large file failed, contents did not match.";
  }
#else
  {
    // Test downloading large file (mobile only), without pausing, as mobile
    // does not support pause during file download, only upload.
    memset(&buffer[0], 0, kLargeFileSize);
    LogDebug("Downloading large file.");
    StorageListener listener;
    firebase::storage::Controller controller;
    firebase::Future<size_t> future =
        ref.GetBytes(&buffer[0], kLargeFileSize, &listener, &controller);
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
  {
    LogDebug("Downloading large file with cancellation.");
    StorageListener listener;
    firebase::storage::Controller controller;
    firebase::Future<size_t> future =
        ref.GetBytes(&buffer[0], kLargeFileSize, &listener, &controller);
    ASSERT_TRUE(controller.is_valid());
    
    // Wait for the transfer to commence.
    while(controller.bytes_transferred() >= 0) {
      ProcessEvents(1);
    }

    LogDebug("Cancelling download.");
    EXPECT_TRUE(controller.Cancel());
    WaitForCompletion(future, "GetBytes", firebase::storage::kErrorCancelled);
  }
}

TEST_F(FirebaseStorageTest, TestLargeFileCancelUpload) {
  SignIn();

  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-LargeFileCancel.txt");

  const size_t kLargeFileSize = kLargeFileMegabytes * 1024 * 1024;
  const std::string kLargeTestFile = CreateDataForLargeFile(kLargeFileSize);
  {
    LogDebug("Write a large file and cancel mid-way.");
    StorageListener listener;
    firebase::storage::Controller controller;
    firebase::Future<firebase::storage::Metadata> future = ref.PutBytes(
        kLargeTestFile.c_str(), kLargeFileSize, &listener, &controller);

    // Ensure the Controller is valid now that we have associated it with an
    // operation.
    ASSERT_TRUE(controller.is_valid());

    ProcessEvents(500);

    LogDebug("Cancelling upload.");
    // Cancel the operation and verify it was successfully canceled.
    EXPECT_TRUE(controller.Cancel());

    WaitForCompletion(future, "PutBytes", firebase::storage::kErrorCancelled);
  }
}

TEST_F(FirebaseStorageTest, TestInvalidatingReferencesWhenDeletingStorage) {
  SignIn();

  // Create a file so we can get its metadata and check that it's properly
  // invalidated.
  firebase::storage::StorageReference ref =
      CreateFolder().Child("TestFile-InvalidateReferencesDeletingStorage.txt");
  // Don't clean up, will be manually deleted.

  WaitForCompletion(ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size()),
                    "PutBytes");
  ASSERT_NE(ref.PutBytesLastResult().result(), nullptr);
  firebase::storage::Metadata metadata = *ref.PutBytesLastResult().result();
  WaitForCompletion(ref.Delete(), "Delete");

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

  WaitForCompletion(ref.PutBytes(&kSimpleTestFile[0], kSimpleTestFile.size()),
                    "PutBytes");
  ASSERT_NE(ref.PutBytesLastResult().result(), nullptr);
  firebase::storage::Metadata metadata = *ref.PutBytesLastResult().result();
  WaitForCompletion(ref.Delete(), "Delete");

  ASSERT_TRUE(ref.is_valid());
  ASSERT_TRUE(metadata.is_valid());
  delete app_;
  app_ = nullptr;
  EXPECT_FALSE(ref.is_valid());
  EXPECT_FALSE(metadata.is_valid());
}

}  // namespace firebase_testapp_automated
