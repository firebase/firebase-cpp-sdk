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
#include "firebase/dynamic_links.h"
#include "firebase/dynamic_links/components.h"
#include "firebase/internal/platform.h"
#include "firebase/log.h"
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
using app_framework::LogInfo;

using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;

// Bundle IDs needed for opening Dynamic Links.
static const char kIOSBundleID[] =
    "com.google.FirebaseCppDynamicLinksTestApp.dev";
static const char kAndroidBundleID[] =
    "com.google.android.dynamiclinks.testapp";
static const char kIOSAppStoreID[] = "2233445566";  // Placeholder ID.

static const char kDomainUriPrefixInvalidError[] =
    "kDomainUriPrefix is not valid, link shortening will fail.\n"
    "To resolve this:\n"
    "* Goto the Firebase console https://firebase.google.com/console/\n"
    "* Click on the Dynamic Links tab\n"
    "* Copy the URI prefix e.g https://a12cd.app.goo.gl or "
    "  https://your-project.page.link\n"
    "* Replace the value of kDomainUriPrefix with the copied URI prefix.\n";

// IMPORTANT: You need to set this to a valid URI prefix from the Firebase
// console (see kDomainUriPrefixInvalidError for the details).
static const char* kDomainUriPrefix = "https://REPLACE_WITH_YOUR_URI_PREFIX";

#define TARGET_URL_PREFIX "https://mysite.example.com"

// When one of the tests tries to open a URL, it suppresses the other tests
// that are attempting to do the same, since only one URL can be opened at a
// time. It does so by setting the "current test" flag to its own test name.
static const char kCurrentTestKey[] = "openurl_current_test";

class TestListener;

class FirebaseDynamicLinksTest : public FirebaseTest {
 public:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

 protected:
  // Try to claim the "current test" flag, returning true if successful and
  // false if not.  Because tests run in sequence, this does not actually
  // require any mutexes. This returns true if it was already claimed by this
  // test, or if no test was claiming it before (in which case, now this test
  // is).
  bool ClaimCurrentTest(const char* test_name);
  // Release the "current test" flag, allowing the next test to run.
  void ReleaseCurrentTest();

  static firebase::App* shared_app_;
  static TestListener* shared_listener_;
  static bool is_desktop_stub_;
  // A list of persistent keys we've saved on the device, to be erased on
  // shutdown after all tests are finished.
  static std::vector<std::string> cleanup_persistent_keys_;
};

firebase::App* FirebaseDynamicLinksTest::shared_app_ = nullptr;
TestListener* FirebaseDynamicLinksTest::shared_listener_ = nullptr;
bool FirebaseDynamicLinksTest::is_desktop_stub_ = false;
// NOLINTNEXTLINE
std::vector<std::string> FirebaseDynamicLinksTest::cleanup_persistent_keys_;

// Handles a received dynamic link.
class TestListener : public firebase::dynamic_links::Listener {
 public:
  TestListener() : received_link_(false) {}
  void OnDynamicLinkReceived(
      const firebase::dynamic_links::DynamicLink* dynamic_link) override {
    LogInfo("Received dynamic link: %s", dynamic_link->url.c_str());
    link_ = *dynamic_link;
    received_link_ = true;
  }

  bool WaitForDynamicLink(firebase::dynamic_links::DynamicLink* link_output) {
    const int kWaitSeconds = 10;
    for (int i = 0; i < kWaitSeconds; i++) {
      ProcessEvents(1000);
      if (received_link_) {
        *link_output = link_;
        return true;
      }
    }
    return false;
  }
  bool received_link_;
  firebase::dynamic_links::DynamicLink link_;
};

void FirebaseDynamicLinksTest::SetUpTestSuite() {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);

  firebase::SetLogLevel(firebase::kLogLevelDebug);
  LogDebug("Initialize Firebase App.");

#if defined(__ANDROID__)
  shared_app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                        app_framework::GetActivity());
#else
  shared_app_ = ::firebase::App::Create();
#endif  // defined(__ANDROID__)

  LogDebug("Initializing Firebase Dynamic Links.");

  shared_listener_ = new TestListener();

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(
      shared_app_, shared_listener_,
      [](::firebase::App* app, void* listener_void) {
        LogDebug("Try to initialize Firebase Dynamic Links");
        firebase::InitResult result;
        result = firebase::dynamic_links::Initialize(
            *app, reinterpret_cast<firebase::dynamic_links::Listener*>(
                      listener_void));
        return result;
      });

  FirebaseTest::WaitForCompletion(initializer.InitializeLastResult(),
                                  "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  is_desktop_stub_ = false;
#if !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  is_desktop_stub_ = true;
#endif  // !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)

  LogDebug("Successfully initialized Firebase Dynamic Links.");
}

void FirebaseDynamicLinksTest::TearDownTestSuite() {
  // On teardown, delete all the persistent keys we should clean up, as long as
  // there is no longer a current test running.
  std::string value;
  if (GetPersistentString(kCurrentTestKey, &value) && !value.empty()) {
    // Don't clean up the persistent keys yet, not until all the tests are done.
    return;
  }
  LogDebug("Tests finished, cleaning up all persistent keys.");
  for (int i = 0; i < cleanup_persistent_keys_.size(); ++i) {
    SetPersistentString(cleanup_persistent_keys_[i].c_str(), nullptr);
  }
  cleanup_persistent_keys_.clear();

  LogDebug("Shutdown Firebase Dynamic Links.");
  firebase::dynamic_links::Terminate();

  delete shared_listener_;
  shared_listener_ = nullptr;

  LogDebug("Shutdown Firebase App.");
  delete shared_app_;
  shared_app_ = nullptr;

  ProcessEvents(100);
}

// Test cases below.

TEST_F(FirebaseDynamicLinksTest, TestInitializeAndTerminate) {
  // Already tested via SetUp() and TearDown().
}

TEST_F(FirebaseDynamicLinksTest, CheckForDomainUriPrefix) {
  ASSERT_EQ(strstr(kDomainUriPrefix, "REPLACE_WITH"), nullptr)
      << kDomainUriPrefixInvalidError;
}

TEST_F(FirebaseDynamicLinksTest, TestCreateLongLink) {
  firebase::dynamic_links::GoogleAnalyticsParameters analytics_parameters;
  analytics_parameters.source = "mysource";
  analytics_parameters.medium = "mymedium";
  analytics_parameters.campaign = "mycampaign";
  analytics_parameters.term = "myterm";
  analytics_parameters.content = "mycontent";

  firebase::dynamic_links::IOSParameters ios_parameters("com.myapp.bundleid");
  ios_parameters.fallback_url = TARGET_URL_PREFIX "/fallback";
  ios_parameters.custom_scheme = "mycustomscheme";
  ios_parameters.minimum_version = "1.2.3";
  ios_parameters.ipad_bundle_id = "com.myapp.bundleid.ipad";
  ios_parameters.ipad_fallback_url = TARGET_URL_PREFIX "/fallbackipad";

  firebase::dynamic_links::ITunesConnectAnalyticsParameters
      app_store_parameters;
  app_store_parameters.affiliate_token = "abcdefg";
  app_store_parameters.campaign_token = "hijklmno";
  app_store_parameters.provider_token = "pq-rstuv";

  firebase::dynamic_links::AndroidParameters android_parameters(
      "com.myapp.packageid");
  android_parameters.fallback_url = TARGET_URL_PREFIX "/fallback";
  android_parameters.minimum_version = 12;

  firebase::dynamic_links::SocialMetaTagParameters social_parameters;
  social_parameters.title = "My App!";
  social_parameters.description = "My app is awesome!";
  social_parameters.image_url = TARGET_URL_PREFIX "/someimage.jpg";

  firebase::dynamic_links::DynamicLinkComponents components(
      "https://google.com/abc", kDomainUriPrefix);
  components.google_analytics_parameters = &analytics_parameters;
  components.ios_parameters = &ios_parameters;
  components.itunes_connect_analytics_parameters = &app_store_parameters;
  components.android_parameters = &android_parameters;
  components.social_meta_tag_parameters = &social_parameters;

  firebase::dynamic_links::GeneratedDynamicLink generated_link =
      firebase::dynamic_links::GetLongLink(components);

  if (is_desktop_stub_) {
    // On desktop, it's enough that we just don't crash.
    SUCCEED();
    return;
  }

  EXPECT_TRUE(generated_link.error.empty());
  EXPECT_NE(generated_link.url, "");
  EXPECT_EQ(generated_link.url.find(kDomainUriPrefix), 0)
      << "Dynamic Link URL (" << generated_link.url
      << ") does not begin with Domain URI Prefix (" << kDomainUriPrefix << ")";
  if (!generated_link.warnings.empty()) {
    LogDebug("GetLongLink warnings:");
    for (auto it = generated_link.warnings.begin();
         it != generated_link.warnings.end(); ++it) {
      LogDebug("  %s", it->c_str());
    }
  }
}

TEST_F(FirebaseDynamicLinksTest, TestGetShortLinkFromComponents) {
  firebase::dynamic_links::GoogleAnalyticsParameters analytics_parameters;
  analytics_parameters.source = "mysource";
  analytics_parameters.medium = "mymedium";
  analytics_parameters.campaign = "mycampaign";
  analytics_parameters.term = "myterm";
  analytics_parameters.content = "mycontent";

  firebase::dynamic_links::IOSParameters ios_parameters("com.myapp.bundleid");
  ios_parameters.fallback_url = TARGET_URL_PREFIX "/fallback";
  ios_parameters.custom_scheme = "mycustomscheme";
  ios_parameters.minimum_version = "1.2.3";
  ios_parameters.ipad_bundle_id = "com.myapp.bundleid.ipad";
  ios_parameters.ipad_fallback_url = TARGET_URL_PREFIX "/fallbackipad";

  firebase::dynamic_links::ITunesConnectAnalyticsParameters
      app_store_parameters;
  app_store_parameters.affiliate_token = "abcdefg";
  app_store_parameters.campaign_token = "hijklmno";
  app_store_parameters.provider_token = "pq-rstuv";

  firebase::dynamic_links::AndroidParameters android_parameters(
      "com.myapp.packageid");
  android_parameters.fallback_url = TARGET_URL_PREFIX "/fallback";
  android_parameters.minimum_version = 12;

  firebase::dynamic_links::SocialMetaTagParameters social_parameters;
  social_parameters.title = "My App!";
  social_parameters.description = "My app is awesome!";
  social_parameters.image_url = TARGET_URL_PREFIX "/someimage.jpg";

  firebase::dynamic_links::DynamicLinkComponents components(
      "https://google.com/def", kDomainUriPrefix);
  components.google_analytics_parameters = &analytics_parameters;
  components.ios_parameters = &ios_parameters;
  components.itunes_connect_analytics_parameters = &app_store_parameters;
  components.android_parameters = &android_parameters;
  components.social_meta_tag_parameters = &social_parameters;

  firebase::Future<firebase::dynamic_links::GeneratedDynamicLink> future;

  FLAKY_TEST_SECTION_BEGIN();  // Occasionally there can be a connection error.
  future = firebase::dynamic_links::GetShortLink(components);
  WaitForCompletion(future, "GetShortLinkFromComponents");
  FLAKY_TEST_SECTION_END();

  if (is_desktop_stub_) {
    // On desktop, it's enough that we just don't crash.
    SUCCEED();
    return;
  }

  const firebase::dynamic_links::GeneratedDynamicLink& generated_link =
      *future.result();

  EXPECT_TRUE(generated_link.error.empty());
  EXPECT_NE(generated_link.url, "");
  EXPECT_EQ(generated_link.url.find(kDomainUriPrefix), 0)
      << "Dynamic Link URL (" << generated_link.url
      << ") does not begin with Domain URI Prefix (" << kDomainUriPrefix << ")";
  if (!generated_link.warnings.empty()) {
    LogDebug("GetShortLinkFromComponents warnings:");
    for (auto it = generated_link.warnings.begin();
         it != generated_link.warnings.end(); ++it) {
      LogDebug("  %s", it->c_str());
    }
  }
}

TEST_F(FirebaseDynamicLinksTest, TestGetShortLinkFromLongLink) {
  firebase::dynamic_links::GoogleAnalyticsParameters analytics_parameters;
  analytics_parameters.source = "mysource";
  analytics_parameters.medium = "mymedium";
  analytics_parameters.campaign = "mycampaign";
  analytics_parameters.term = "myterm";
  analytics_parameters.content = "mycontent";

  firebase::dynamic_links::IOSParameters ios_parameters("com.myapp.bundleid");
  ios_parameters.fallback_url = TARGET_URL_PREFIX "/fallback";
  ios_parameters.custom_scheme = "mycustomscheme";
  ios_parameters.minimum_version = "1.2.3";
  ios_parameters.ipad_bundle_id = "com.myapp.bundleid.ipad";
  ios_parameters.ipad_fallback_url = TARGET_URL_PREFIX "/fallbackipad";

  firebase::dynamic_links::ITunesConnectAnalyticsParameters
      app_store_parameters;
  app_store_parameters.affiliate_token = "abcdefg";
  app_store_parameters.campaign_token = "hijklmno";
  app_store_parameters.provider_token = "pq-rstuv";

  firebase::dynamic_links::AndroidParameters android_parameters(
      "com.myapp.packageid");
  android_parameters.fallback_url = TARGET_URL_PREFIX "/fallback";
  android_parameters.minimum_version = 12;

  firebase::dynamic_links::SocialMetaTagParameters social_parameters;
  social_parameters.title = "My App!";
  social_parameters.description = "My app is awesome!";
  social_parameters.image_url = TARGET_URL_PREFIX "/someimage.jpg";

  firebase::dynamic_links::DynamicLinkComponents components(
      "https://google.com/ghi", kDomainUriPrefix);
  components.google_analytics_parameters = &analytics_parameters;
  components.ios_parameters = &ios_parameters;
  components.itunes_connect_analytics_parameters = &app_store_parameters;
  components.android_parameters = &android_parameters;
  components.social_meta_tag_parameters = &social_parameters;

  firebase::dynamic_links::GeneratedDynamicLink long_link =
      firebase::dynamic_links::GetLongLink(components);

  if (is_desktop_stub_) {
    // On desktop, it's enough that we just don't crash.
    SUCCEED();
    return;
  }

  EXPECT_NE(long_link.url, "");

  firebase::dynamic_links::DynamicLinkOptions options;
  options.path_length = firebase::dynamic_links::kPathLengthShort;
  firebase::Future<firebase::dynamic_links::GeneratedDynamicLink> future;

  FLAKY_TEST_SECTION_BEGIN();  // Occasional connection errors.
  future =
      firebase::dynamic_links::GetShortLink(long_link.url.c_str(), options);
  WaitForCompletion(future, "GetShortLinkFromLongLink");
  FLAKY_TEST_SECTION_END();

  const firebase::dynamic_links::GeneratedDynamicLink& generated_link =
      *future.result();

  EXPECT_TRUE(generated_link.error.empty());
  EXPECT_NE(generated_link.url, "");
  EXPECT_EQ(generated_link.url.find(kDomainUriPrefix), 0)
      << "Dynamic Link URL (" << generated_link.url
      << ") does not begin with Domain URI Prefix (" << kDomainUriPrefix << ")";
  if (!generated_link.warnings.empty()) {
    LogDebug("GetShortLinkFromLongLink warnings:");
    for (auto it = generated_link.warnings.begin();
         it != generated_link.warnings.end(); ++it) {
      LogDebug("  %s", it->c_str());
    }
  }
}

bool FirebaseDynamicLinksTest::ClaimCurrentTest(const char* test_name) {
  // Tests using OpenUrlInBrowser must be run one at a time per run of the app.
  // The workflow for these tests is:
  //
  // Run #1: Test A opens its link in browser, tests B & C do nothing.
  // Run #2: Test A verifies that its link loaded, test B opens its link in
  //         browser, test C does nothing.
  // Run #3: Test A remembers whether its link had loaded, test B verifies that
  //         its link loaded, test C opens its link in browser.
  // Run #4: Tests A & B remember whether their links had loaded, test C
  //         verifies that its link loaded.
  //
  // This is accomplished by setting the value of kCurrentTestKey, which tells
  // us which of the tests is currently doing its thing. Each test can also set
  // a state variable saying whether they are opening the link in browser (the
  // starting state), verifying that the link opened, or previously opened (or
  // failed to open) the link. Tests that previously failed to open the link
  // will continue to register a FAIL until all the tests are finished.
  std::string value;
  if (!GetPersistentString(kCurrentTestKey, &value) || value == test_name) {
    // If not already set to it, take ownership of the current test.
    if (value != test_name) {
      SetPersistentString(kCurrentTestKey, test_name);
    }
    return true;
  }
  return false;
}

void FirebaseDynamicLinksTest::ReleaseCurrentTest() {
  SetPersistentString(kCurrentTestKey, nullptr);
}

static firebase::dynamic_links::DynamicLinkComponents GenerateComponentsForTest(
    const char* url) {
  static firebase::dynamic_links::AndroidParameters android_parameters(
      kAndroidBundleID);
  static firebase::dynamic_links::IOSParameters ios_parameters(kIOSBundleID);
  ios_parameters.app_store_id = kIOSAppStoreID;
  static firebase::dynamic_links::SocialMetaTagParameters social_parameters;
  static firebase::dynamic_links::ITunesConnectAnalyticsParameters
      app_store_parameters;
  static firebase::dynamic_links::GoogleAnalyticsParameters
      analytics_parameters;
  firebase::dynamic_links::DynamicLinkComponents components(url,
                                                            kDomainUriPrefix);
  components.google_analytics_parameters = &analytics_parameters;
  components.ios_parameters = &ios_parameters;
  components.itunes_connect_analytics_parameters = &app_store_parameters;
  components.android_parameters = &android_parameters;
  components.social_meta_tag_parameters = &social_parameters;
  return components;
}
static const char kStateSentLink[] = "sentLink";
static const char kStateReceivedLink[] = "receivedLink";
static const char kStateReceivedLinkFail[] = "receivedLinkFail";

TEST_F(FirebaseDynamicLinksTest, TestOpeningLongLinkInRunningApp) {
  // On iOS, the dynamic link landing page requires a click.
  // On Android, the first time a dynamic link is clicked on the device, Google
  // Play services shows a TOS popup. Either way, this test requires user
  // interaction.
#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || defined(__ANDROID__)
  TEST_REQUIRES_USER_INTERACTION;
#endif  // (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) ||
        // defined(__ANDROID__)

  // This test uses a persistent key to keep track of how it's running.
  const char kUrlToOpen[] = "https://google.com/test_opening_long_link";
  std::string persistent_key_str = test_info_->name();
  const char* persistent_key = persistent_key_str.c_str();

  bool owns_current_test = ClaimCurrentTest(persistent_key);

  cleanup_persistent_keys_.push_back(persistent_key);
  std::string value;
  if (owns_current_test && !GetPersistentString(persistent_key, &value)) {
    // The first time, create a dynamic link and open it in the browser.
    LogDebug("First run, creating and opening long dynamic link...");

    firebase::dynamic_links::DynamicLinkComponents components =
        GenerateComponentsForTest(kUrlToOpen);
    firebase::dynamic_links::GeneratedDynamicLink link =
        firebase::dynamic_links::GetLongLink(components);

    if (is_desktop_stub_) {
      // On desktop, it's enough that we just don't crash.
      LogDebug("Succeeded as stub.");
      SUCCEED();
      return;
    }
    EXPECT_TRUE(link.error.empty());
    SetPersistentString(persistent_key, kStateSentLink);
    // This will trigger the test to start over.
    OpenUrlInBrowser(link.url.c_str());
    exit(0);  // Kill the app after opening the URL so it can be restarted
              // properly.
  } else if (owns_current_test && GetPersistentString(persistent_key, &value) &&
             value == kStateSentLink) {
    // The second time, check that we received the dynamic link.
    LogDebug("Second run, checking for dynamic link...");
    firebase::dynamic_links::DynamicLink got_link;
    EXPECT_TRUE(shared_listener_->WaitForDynamicLink(&got_link));
    EXPECT_EQ(got_link.url, kUrlToOpen);
    if (got_link.url == kUrlToOpen) {
      SetPersistentString(persistent_key, kStateReceivedLink);
    } else {
      SetPersistentString(persistent_key, kStateReceivedLinkFail);
    }
    ReleaseCurrentTest();
  } else if (GetPersistentString(persistent_key, &value) &&
             value == kStateReceivedLink) {
    // Already verified the link was correct.
    LogDebug("Previously verified that dynamic link was received.");
    SUCCEED();
  } else if (GetPersistentString(persistent_key, &value) &&
             value == kStateReceivedLinkFail) {
    // Already verified the link failed.
    FAIL() << "Previous attempt to get link failed.";
  } else {
    LogDebug("Skipping this test because another test has taken ownership.");
    SUCCEED();
  }
}

TEST_F(FirebaseDynamicLinksTest, TestOpeningShortLinkFromLongLinkInRunningApp) {
  // On iOS, the dynamic link landing page requires a click.
  // On Android, the first time a dynamic link is clicked on the device, Google
  // Play services shows a TOS popup. Either way, this test requires user
  // interaction.
#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || defined(__ANDROID__)
  TEST_REQUIRES_USER_INTERACTION;
#endif  // (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) ||
        // defined(__ANDROID__)

  // This test uses a persistent key to keep track of how it's running.
  const char kUrlToOpen[] =
      "https://google.com/test_opening_short_link_from_long_link";
  std::string persistent_key_str = test_info_->name();
  const char* persistent_key = persistent_key_str.c_str();

  bool owns_current_test = ClaimCurrentTest(persistent_key);

  cleanup_persistent_keys_.push_back(persistent_key);
  std::string value;
  if (owns_current_test && !GetPersistentString(persistent_key, &value)) {
    // The first time, create a dynamic link and open it in the browser.
    LogDebug(
        "First run, creating and opening short dynamic link from long link...");
    firebase::dynamic_links::DynamicLinkComponents components =
        GenerateComponentsForTest(kUrlToOpen);
    firebase::dynamic_links::GeneratedDynamicLink long_link =
        firebase::dynamic_links::GetLongLink(components);
    // Shorten link.
    firebase::dynamic_links::DynamicLinkOptions options;
    options.path_length = firebase::dynamic_links::kPathLengthShort;
    firebase::Future<firebase::dynamic_links::GeneratedDynamicLink> future;

    FLAKY_TEST_SECTION_BEGIN();  // Occasional connection errors.
    future =
        firebase::dynamic_links::GetShortLink(long_link.url.c_str(), options);
    WaitForCompletion(future, "GetShortLinkFromLongLink");
    FLAKY_TEST_SECTION_END();

    if (is_desktop_stub_) {
      // On desktop, it's enough that we just don't crash.
      LogDebug("Succeeded as stub.");
      SUCCEED();
      return;
    }
    const firebase::dynamic_links::GeneratedDynamicLink& link =
        *future.result();

    EXPECT_TRUE(link.error.empty());
    SetPersistentString(persistent_key, kStateSentLink);
    // This will trigger the test to start over.
    OpenUrlInBrowser(link.url.c_str());
    exit(0);  // Kill the app after opening the URL so it can be restarted
              // properly;
  } else if (owns_current_test && GetPersistentString(persistent_key, &value) &&
             value == kStateSentLink) {
    // The second time, check that we received the dynamic link.
    LogDebug("Second run, checking for dynamic link...");
    firebase::dynamic_links::DynamicLink got_link;
    EXPECT_TRUE(shared_listener_->WaitForDynamicLink(&got_link));
    EXPECT_EQ(got_link.url, kUrlToOpen);
    if (got_link.url == kUrlToOpen) {
      SetPersistentString(persistent_key, kStateReceivedLink);
    } else {
      SetPersistentString(persistent_key, kStateReceivedLinkFail);
    }
    ReleaseCurrentTest();
  } else if (GetPersistentString(persistent_key, &value) &&
             value == kStateReceivedLink) {
    // Already verified the link was correct.
    LogDebug("Previously verified that dynamic link was received.");
    SUCCEED();
  } else if (GetPersistentString(persistent_key, &value) &&
             value == kStateReceivedLinkFail) {
    // Already verified the link failed.
    FAIL() << "Previous attempt to get link failed.";
  } else {
    LogDebug("Skipping this test because another test has taken ownership.");
    SUCCEED();
  }
}

TEST_F(FirebaseDynamicLinksTest,
       TestOpeningShortLinkFromComponentsInRunningApp) {
  // On iOS, the dynamic link landing page requires a click.
  // On Android, the first time a dynamic link is clicked on the device, Google
  // Play services shows a TOS popup. Either way, this test requires user
  // interaction.
#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || defined(__ANDROID__)
  TEST_REQUIRES_USER_INTERACTION;
#endif  // (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) ||
        // defined(__ANDROID__)

  // This test uses a persistent key to keep track of how it's running.
  const char kUrlToOpen[] =
      "https://google.com/test_opening_short_link_from_components";
  std::string persistent_key_str = test_info_->name();
  const char* persistent_key = persistent_key_str.c_str();

  bool owns_current_test = ClaimCurrentTest(persistent_key);

  cleanup_persistent_keys_.push_back(persistent_key);
  std::string value;
  if (owns_current_test && !GetPersistentString(persistent_key, &value)) {
    // The first time, create a dynamic link and open it in the browser.
    LogDebug(
        "First run, creating and opening short dynamic link from "
        "components...");
    firebase::dynamic_links::DynamicLinkComponents components =
        GenerateComponentsForTest(kUrlToOpen);

    firebase::Future<firebase::dynamic_links::GeneratedDynamicLink> future;
    FLAKY_TEST_SECTION_BEGIN();  // Occasional connection errors.
    future = firebase::dynamic_links::GetShortLink(components);
    WaitForCompletion(future, "GetShortLinkFromLongLink");
    FLAKY_TEST_SECTION_END();  // Occasional connection errors.

    if (is_desktop_stub_) {
      // On desktop, it's enough that we just don't crash.
      LogDebug("Succeeded as stub.");
      SUCCEED();
      return;
    }

    const firebase::dynamic_links::GeneratedDynamicLink& link =
        *future.result();

    EXPECT_TRUE(link.error.empty());
    SetPersistentString(persistent_key, kStateSentLink);
    // This will trigger the test to start over.
    OpenUrlInBrowser(link.url.c_str());
    exit(0);  // Kill the app after opening the URL so it can be restarted
              // properly.
  } else if (owns_current_test && GetPersistentString(persistent_key, &value) &&
             value == kStateSentLink) {
    // The second time, check that we received the dynamic link.
    LogDebug("Second run, checking for dynamic link...");
    firebase::dynamic_links::DynamicLink got_link;
    EXPECT_TRUE(shared_listener_->WaitForDynamicLink(&got_link));
    EXPECT_EQ(got_link.url, kUrlToOpen);
    if (got_link.url == kUrlToOpen) {
      SetPersistentString(persistent_key, kStateReceivedLink);
    } else {
      SetPersistentString(persistent_key, kStateReceivedLinkFail);
    }
    ReleaseCurrentTest();
  } else if (GetPersistentString(persistent_key, &value) &&
             value == kStateReceivedLink) {
    // Already verified the link was correct.
    LogDebug("Previously verified that dynamic link was received.");
    SUCCEED();
  } else if (GetPersistentString(persistent_key, &value) &&
             value == kStateReceivedLinkFail) {
    // Already verified the link failed.
    FAIL() << "Previous attempt to get link failed.";
  } else {
    LogDebug("Skipping this test because another test has taken ownership.");
    SUCCEED();
  }
}
}  // namespace firebase_testapp_automated
