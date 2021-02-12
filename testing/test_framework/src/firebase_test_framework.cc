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

const int kRetryDelaysMs[] = {
  // Roughly exponential backoff for the retries.
  100, 1000, 5000, 10000, 30000
};
const int kMaxRetries = sizeof(kRetryDelaysMs) / sizeof(kRetryDelaysMs[0]);

firebase::FutureBase FirebaseTest::RunWithRetryBase(
    firebase::FutureBase (*run_future)(void* context),
    void* context, const char* name, int expected_error) {

  int attempt = 0;
  firebase::FutureBase future;

  while (attempt < kMaxRetries) {
    attempt++;
    future = run_future(context);
    if (attempt >= kMaxRetries) {
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
          *name?" ":"",
          name, attempt);
    }
    else if (future.error() != expected_error) {
      app_framework::LogDebug(
          "RunWithRetry%s%s: Attempt %d returned error %d, expected %d",
          *name?" ":"",
          name, attempt, future.error(), expected_error);
    }
    else {
      // Future is completed and the error matches what's expected, no need to
      // retry further.
      break;
    }
    int delay_ms = kRetryDelaysMs[attempt-1];
    app_framework::LogDebug(
        "RunWithRetry%s%s: Pause %d milliseconds before retrying.",
        *name?" ":"",
        name, delay_ms);
    app_framework::ProcessEvents(delay_ms);
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

bool FirebaseTest::IsUserInteractionAllowed() {
  // In the trivial case, just check whether we are logging to file. If not,
  // assume interaction is allowed.
  return !app_framework::IsLoggingToFile();
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

extern "C" int common_main(int argc, char* argv[]) {
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
