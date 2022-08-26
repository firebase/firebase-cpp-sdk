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

#include "firebase_test_framework.h"  // NOLINT

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "firebase/future.h"

namespace firebase {
namespace internal {
// Borrow Firebase's internal Base64 encoder and decoder.
extern bool Base64Encode(const std::string& input, std::string* output);
extern bool Base64Decode(const std::string& input, std::string* output);
}  // namespace internal
}  // namespace firebase

namespace firebase_test_framework {

int FirebaseTest::argc_ = 0;
char** FirebaseTest::argv_ = nullptr;
bool FirebaseTest::found_config_ = false;

FirebaseTest::FirebaseTest() : app_(nullptr) {}

FirebaseTest::~FirebaseTest() { assert(app_ == nullptr); }

void FirebaseTest::SetUp() {}

void FirebaseTest::TearDown() {
  if (HasFailure()) {
    app_framework::SetPreserveFullLog(false);
    app_framework::LogError(
        "Test %s failed.\nFull test log:\n%s",
        ::testing::UnitTest::GetInstance()->current_test_info()->name(),
        "========================================================");
    app_framework::SetPreserveFullLog(true);
    app_framework::AddToFullLog(
        "========================================================\n");
    app_framework::OutputFullLog();
  } else {
    app_framework::ClearFullLog();
  }
}

void FirebaseTest::FindFirebaseConfig(const char* try_directory) {
#if !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  static const char kDefaultGoogleServicesPath[] = "google-services.json";

  if (!found_config_) {
    if (try_directory[0] != '\0' && app_framework::FileExists(try_directory)) {
      app_framework::ChangeToFileDirectory(try_directory);
    } else if (app_framework::FileExists(kDefaultGoogleServicesPath)) {
      // It's in the current directory, don't do anything.
    } else {
      // Try the directory the binary is in.
      app_framework::ChangeToFileDirectory(argv_[0]);
    }
    // Only do this once.
    found_config_ = true;
  }
#endif  //  !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) &&
        //  TARGET_OS_IPHONE)
  (void)try_directory;
}

void FirebaseTest::InitializeApp() {
  if (app_) return;  // Already initialized.

  app_framework::LogDebug("Initialize Firebase App.");

#if defined(__ANDROID__)
  app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                 app_framework::GetActivity());
#else
  app_ = ::firebase::App::Create();
#endif  // defined(__ANDROID__)
}

void FirebaseTest::TerminateApp() {
  if (!app_) return;  // Already terminated.

  app_framework::LogDebug("Shutdown Firebase App.");
  delete app_;
  app_ = nullptr;
}

bool FirebaseTest::RunFlakyBlockBase(bool (*flaky_block)(void* context),
                                     void* context, const char* name) {
  // Run flaky_block(context). If it returns true, all is well, return true.
  // If it returns false, something flaky failed; wait a moment and try again.
  const int kRetryDelaysMs[] = {// Roughly exponential backoff for the retries.
                                100, 1000, 5000, 10000, 30000};
  const int kNumAttempts =
      1 + (sizeof(kRetryDelaysMs) / sizeof(kRetryDelaysMs[0]));

  int attempt = 0;

  while (attempt < kNumAttempts) {
    bool result = flaky_block(context);
    if (result || (attempt == kNumAttempts - 1)) {
      return result;
    }
    app_framework::LogDebug("RunFlakyBlock%s%s: Attempt %d failed",
                            *name ? " " : "", name, attempt + 1);
    int delay_ms = kRetryDelaysMs[attempt];
    app_framework::ProcessEvents(delay_ms);
    attempt++;
  }
  return false;
}

firebase::FutureBase FirebaseTest::RunWithRetryBase(
    firebase::FutureBase (*run_future)(void* context), void* context,
    const char* name, int expected_error) {
  // Run run_future(context), which returns a Future, then wait for that Future
  // to complete. If the Future returns Invalid, or if its error() does
  // not match expected_error, pause a moment and try again.
  //
  // In most cases, this will return the Future once it's been completed.
  // However, if it reaches the last attempt, it will return immediately once
  // the operation begins. This is because at this point we want to return the
  // results whether or not the operation succeeds.
  const int kRetryDelaysMs[] = {// Roughly exponential backoff for the retries.
                                100, 1000, 5000, 10000, 30000};
  const int kNumAttempts =
      1 + (sizeof(kRetryDelaysMs) / sizeof(kRetryDelaysMs[0]));

  int attempt = 0;
  firebase::FutureBase future;

  while (attempt < kNumAttempts) {
    future = run_future(context);
    if (attempt == kNumAttempts - 1) {
      // This is the last attempt, return immediately.
      break;
    }

    // Wait for completion, then check status and error.
    while (future.status() == firebase::kFutureStatusPending) {
      app_framework::ProcessEvents(100);
    }
    if (future.status() != firebase::kFutureStatusComplete) {
      app_framework::LogDebug(
          "RunWithRetry%s%s: Attempt %d returned invalid status",
          *name ? " " : "", name, attempt + 1);
    } else if (future.error() != expected_error) {
      app_framework::LogDebug(
          "RunWithRetry%s%s: Attempt %d returned error %d, expected %d",
          *name ? " " : "", name, attempt + 1, future.error(), expected_error);
    } else {
      // Future is completed and the error matches what's expected, no need to
      // retry further.
      break;
    }
    int delay_ms = kRetryDelaysMs[attempt];
    app_framework::LogDebug(
        "RunWithRetry%s%s: Pause %d milliseconds before retrying.",
        *name ? " " : "", name, delay_ms);
    app_framework::ProcessEvents(delay_ms);
    attempt++;
  }
  return future;
}

bool FirebaseTest::WaitForCompletion(const firebase::FutureBase& future,
                                     const char* name, int expected_error) {
  app_framework::LogDebug("WaitForCompletion %s", name);
  while (future.status() == firebase::kFutureStatusPending) {
    app_framework::ProcessEvents(100);
  }
  EXPECT_EQ(future.status(), firebase::kFutureStatusComplete)
      << name << " returned an invalid status.";
  EXPECT_EQ(future.error(), expected_error)
      << name << " returned error " << future.error() << ": "
      << future.error_message();
  return (future.status() == firebase::kFutureStatusComplete &&
          future.error() == expected_error);
}

bool FirebaseTest::WaitForCompletionAnyResult(
    const firebase::FutureBase& future, const char* name) {
  app_framework::LogDebug("WaitForCompletion %s", name);
  while (future.status() == firebase::kFutureStatusPending) {
    app_framework::ProcessEvents(100);
  }
  EXPECT_EQ(future.status(), firebase::kFutureStatusComplete)
      << name << " returned an invalid status.";
  return (future.status() == firebase::kFutureStatusComplete);
}

static void VariantToStringInternal(const firebase::Variant& variant,
                                    std::ostream& out,
                                    const std::string& indent) {
  if (variant.is_null()) {
    out << "null";
  } else if (variant.is_int64()) {
    out << variant.int64_value();
  } else if (variant.is_double()) {
    out << variant.double_value();
  } else if (variant.is_bool()) {
    out << (variant.bool_value() ? "true" : "false");
  } else if (variant.is_string()) {
    out << variant.string_value();
  } else if (variant.is_blob()) {
    out << "blob[" << variant.blob_size() << "] = <";
    char hex[3];
    for (size_t i = 0; i < variant.blob_size(); ++i) {
      snprintf(hex, sizeof(hex), "%02x", variant.blob_data()[i]);
      if (i != 0) out << " ";
      out << hex;
    }
    out << ">";
  } else if (variant.is_vector()) {
    out << "[" << std::endl;
    const auto& v = variant.vector();
    for (auto it = v.begin(); it != v.end(); ++it) {
      out << indent + "  ";
      VariantToStringInternal(*it, out, indent + "  ");
      auto next_it = it;
      next_it++;
      if (next_it != v.end()) out << ",";
      out << std::endl;
    }
    out << "]";
  } else if (variant.is_map()) {
    out << "[" << std::endl;
    const auto& m = variant.map();
    for (auto it = m.begin(); it != m.end(); ++it) {
      out << indent + "  ";
      VariantToStringInternal(it->first, out, indent + "  ");
      out << ": ";
      VariantToStringInternal(it->second, out, indent + "  ");
      auto next_it = it;
      next_it++;
      if (next_it != m.end()) out << ",";
      out << std::endl;
    }
    out << "]";
  } else {
    out << "<unsupported type>";
  }
}

std::string FirebaseTest::VariantToString(const firebase::Variant& variant) {
  std::ostringstream out;
  VariantToStringInternal(variant, out, "");
  return out.str();
}

bool FirebaseTest::ShouldRunUITests() {
  return app_framework::ShouldRunUITests();
}

bool FirebaseTest::ShouldRunNonUITests() {
  return app_framework::ShouldRunNonUITests();
}

bool FirebaseTest::Base64Encode(const std::string& input, std::string* output) {
  return ::firebase::internal::Base64Encode(input, output);
}

bool FirebaseTest::Base64Decode(const std::string& input, std::string* output) {
  return ::firebase::internal::Base64Decode(input, output);
}

class LogTestEventListener : public testing::EmptyTestEventListener {
 public:
  void OnTestPartResult(
      const testing::TestPartResult& test_part_result) override {
    if (test_part_result.failed() && test_part_result.message()) {
      app_framework::AddToFullLog(test_part_result.message());
      app_framework::AddToFullLog("\n");
    }
  };
};

}  // namespace firebase_test_framework

namespace firebase {
// gtest requires that the operator<< be in the same namespace as the item you
// are outputting.
std::ostream& operator<<(std::ostream& os, const Variant& v) {
  return os << firebase_test_framework::FirebaseTest::VariantToString(v);
}
}  // namespace firebase

namespace {

std::vector<std::string> ArgcArgvToVector(int argc, char* argv[]) {
  std::vector<std::string> args_vector;
  for (int i = 0; i < argc; ++i) {
    args_vector.push_back(argv[i]);
  }
  return args_vector;
}

char** VectorToArgcArgv(const std::vector<std::string>& args_vector,
                        int* argc) {
  // Ensure that `argv` ends with a null terminator. This is a POSIX requirement
  // (see https://man7.org/linux/man-pages/man2/execve.2.html) and googletest
  // relies on it. Without this null terminator, the
  // `ParseGoogleTestFlagsOnlyImpl()` function in googletest accesses invalid
  // memory and causes an Address Sanitizer failure.
  char** argv = new char*[args_vector.size() + 1];
  for (int i = 0; i < args_vector.size(); ++i) {
    const char* arg = args_vector[i].c_str();
    char* arg_copy = new char[std::strlen(arg) + 1];
    std::strcpy(arg_copy, arg);
    argv[i] = arg_copy;
  }
  argv[args_vector.size()] = nullptr;
  *argc = static_cast<int>(args_vector.size());
  return argv;
}

/**
 * Makes changes to argc and argv before passing them to `InitGoogleTest`.
 *
 * This function is a convenience function for developers to edit during
 * development/debugging to customize the the arguments specified to googletest
 * when directly specifying command-line arguments is not available, such as on
 * Android and iOS. For example, to debug a specific test, add the
 * --gtest_filter argument, and to list all tests add the --gtest_list_tests
 * argument.
 *
 * @param argc A pointer to the `argc` that will be specified to
 * `InitGoogleTest`; the integer to which this pointer points will be updated
 * with the new length of `argv`.
 * @param argv The `argv` that contains the arguments that would have otherwise
 * been specified to `InitGoogleTest()`; they will not be modified.
 *
 * @return The new `argv` to be specified to `InitGoogleTest()`.
 */
char** EditMainArgsForGoogleTest(int* argc, char* argv[]) {
  // Put the args into a vector of strings because modifying string objects in
  // a vector is far easier than modifying a char** array.
  const std::vector<std::string> original_args = ArgcArgvToVector(*argc, argv);
  std::vector<std::string> modified_args(original_args);

  // Add elements to the `modified_args` vector to specify to googletest.
  // e.g. modified_args.push_back("--gtest_list_tests");
  // e.g. modified_args.push_back("--gtest_filter=MyTestFixture.MyTest");

  // Disable googletest's exception handling logic when debugging test failures
  // due to exceptions. This can be helpful because when exceptions are handled
  // by googletest (the default) the stack traces are lost; however, when they
  // are instead allowed to bubble up and crash the app then helpful stack
  // traces are usually included as part of the crash dump.
  // See https://goo.gle/2WcC3fV for details.
  // modified_args.push_back("--gtest_catch_exceptions=0");

  // Create a new `argv` with the elements from the `modified_args` vector and
  // write the new count back to `argc`. The memory leaks produced by
  // `VectorToArgcArgv` acceptable because they last for the entire application.
  // Calling `VectorToArgcArgv` also fixes an invalid memory access performed by
  // googletest by adding the required null element to the end of `argv`.
  return VectorToArgcArgv(modified_args, argc);
}

}  // namespace

extern "C" int common_main(int argc, char* argv[]) {
  argv = EditMainArgsForGoogleTest(&argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  firebase_test_framework::FirebaseTest::SetArgs(argc, argv);
  app_framework::SetLogLevel(app_framework::kDebug);
  // Anything below the given log level will be preserved, and printed out in
  // the event of test failure.
  app_framework::SetPreserveFullLog(true);
  ::testing::TestEventListeners& listeners =
      ::testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new firebase_test_framework::LogTestEventListener());
  int result = RUN_ALL_TESTS();

  return result;
}
