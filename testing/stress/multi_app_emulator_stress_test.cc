/*
 * Copyright 2026 Google LLC
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

// Emulator-backed desktop multi-App lifecycle stress tests reproducing
// jwinarske's November 2026 desktop reports (#1969, #1970, #1971, #1972,
// #1973, #1974) under ASan/UBSan and TSan.
//
// Intended to be invoked under:
//   firebase emulators:exec --only auth,firestore,database,storage \
//     --project demo-example <path-to-binary>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app_check/src/include/firebase/app_check.h"
#include "auth/src/include/firebase/auth.h"
#include "database/src/include/firebase/database.h"
#include "firestore/src/include/firebase/firestore.h"
#include "gtest/gtest.h"
#include "storage/src/include/firebase/storage.h"

namespace firebase {
namespace {

std::pair<std::string, int> ParseHostPort(const char* env_var,
                                          const std::string& default_host,
                                          int default_port) {
  const char* raw = std::getenv(env_var);
  if (!raw || raw[0] == '\0') {
    return {default_host, default_port};
  }
  std::string value(raw);
  auto colon = value.rfind(':');
  if (colon == std::string::npos) {
    return {value, default_port};
  }
  return {value.substr(0, colon), std::atoi(value.substr(colon + 1).c_str())};
}

int GetStressRounds(int default_rounds) {
  const char* raw = std::getenv("FIREBASE_STRESS_ROUNDS");
  if (!raw || raw[0] == '\0') return default_rounds;
  int parsed = std::atoi(raw);
  return parsed > 0 ? parsed : default_rounds;
}

AppOptions MakeEmulatorAppOptions() {
  auto [db_host, db_port] =
      ParseHostPort("FIREBASE_DATABASE_EMULATOR_HOST", "127.0.0.1", 9000);
  AppOptions options;
  options.set_app_id("1:1234567890:android:0123456789abcdef");
  options.set_api_key("fake-api-key-for-emulator");
  options.set_project_id("demo-example");
  options.set_storage_bucket("demo-example.appspot.com");
  std::string db_url = "http://" + db_host + ":" + std::to_string(db_port) +
                       "/?ns=demo-example";
  options.set_database_url(db_url.c_str());
  return options;
}

void ConfigureAuthEmulator(auth::Auth* auth) {
  auto [host, port] =
      ParseHostPort("FIREBASE_AUTH_EMULATOR_HOST", "127.0.0.1", 9099);
  auth->UseEmulator(host, static_cast<uint32_t>(port));
}

void ConfigureFirestoreEmulator(firestore::Firestore* fs) {
  auto [host, port] =
      ParseHostPort("FIRESTORE_EMULATOR_HOST", "127.0.0.1", 8080);
  auto settings = fs->settings();
  settings.set_host(host + ":" + std::to_string(port));
  settings.set_ssl_enabled(false);
  fs->set_settings(settings);
}

void ConfigureStorageEmulator(storage::Storage* store) {
  auto [host, port] =
      ParseHostPort("FIREBASE_STORAGE_EMULATOR_HOST", "127.0.0.1", 9199);
  store->UseEmulator(host, port);
}

// Reproduces #1972 (Auth::SignOut <-> Firestore GetToken deadlock),
// #1973 (deleting Firestore with pending write frees CleanupNotifier before
// Promise), and #1974 (Firestore aborts in RequestToken on concurrent SignOut).
TEST(MultiAppEmulatorStressTest, AuthSignOutRacesFirestoreGetSetAndTeardown) {
  const int rounds = GetStressRounds(50);
  for (int i = 0; i < rounds; ++i) {
    std::string app_name = "fs_race_" + std::to_string(i);
    std::unique_ptr<App> app(
        App::Create(MakeEmulatorAppOptions(), app_name.c_str()));
    ASSERT_NE(app, nullptr);

    std::unique_ptr<auth::Auth> auth(auth::Auth::GetAuth(app.get()));
    ASSERT_NE(auth, nullptr);
    ConfigureAuthEmulator(auth.get());

    std::unique_ptr<firestore::Firestore> fs(
        firestore::Firestore::GetInstance(app.get()));
    ASSERT_NE(fs, nullptr);
    ConfigureFirestoreEmulator(fs.get());

    auto sign_in = auth->SignInAnonymously();
    ASSERT_TRUE(sign_in.Wait(15000))
        << "SignInAnonymously timed out on round " << i;
    ASSERT_EQ(sign_in.error(), auth::kAuthErrorNone)
        << "SignInAnonymously failed on round " << i << ": "
        << sign_in.error_message();

    auto doc = fs->Document("stress/doc_" + std::to_string(i));
    auto get_future = doc.Get();
    auto set_future =
        doc.Set({{"round", firestore::FieldValue::Integer(i)},
                 {"tag", firestore::FieldValue::String("stress")}});

    // Do not wait for get_future / set_future; immediately sign out and tear
    // down Firestore, Auth, and App.
    auth->SignOut();
    fs.reset();
    auth.reset();
    app.reset();
  }
}

// Reproduces #1969 with live Auth + emulator: keep default App + Auth alive,
// sign in/out on a second App, and immediately destroy the second Auth + App.
TEST(MultiAppEmulatorStressTest, TwoAppAuthSignOutAndDeleteRace) {
  std::unique_ptr<App> app_a(App::Create(MakeEmulatorAppOptions()));
  ASSERT_NE(app_a, nullptr);
  std::unique_ptr<auth::Auth> auth_a(auth::Auth::GetAuth(app_a.get()));
  ASSERT_NE(auth_a, nullptr);
  ConfigureAuthEmulator(auth_a.get());

  const int rounds = GetStressRounds(25);
  for (int i = 0; i < rounds; ++i) {
    std::string app_name = "auth_second_" + std::to_string(i);
    std::unique_ptr<App> app_b(
        App::Create(MakeEmulatorAppOptions(), app_name.c_str()));
    ASSERT_NE(app_b, nullptr);
    std::unique_ptr<auth::Auth> auth_b(auth::Auth::GetAuth(app_b.get()));
    ASSERT_NE(auth_b, nullptr);
    ConfigureAuthEmulator(auth_b.get());

    auto sign_in = auth_b->SignInAnonymously();
    ASSERT_TRUE(sign_in.Wait(15000))
        << "SignInAnonymously timed out on round " << i;
    ASSERT_EQ(sign_in.error(), auth::kAuthErrorNone)
        << "SignInAnonymously failed on round " << i << ": "
        << sign_in.error_message();

    auth_b->SignOut();
    auth_b.reset();
    app_b.reset();
  }
}

// Exercises 2 concurrent Apps with Auth + Storage + Firestore pointed at the
// emulators, verifying clean teardown and surfacing cross-module data races.
TEST(MultiAppEmulatorStressTest, TwoAppMultiProductEmulatorTeardown) {
  std::unique_ptr<App> app_a(App::Create(MakeEmulatorAppOptions()));
  ASSERT_NE(app_a, nullptr);
  std::unique_ptr<auth::Auth> auth_a(auth::Auth::GetAuth(app_a.get()));
  ASSERT_NE(auth_a, nullptr);
  ConfigureAuthEmulator(auth_a.get());
  std::unique_ptr<storage::Storage> store_a(
      storage::Storage::GetInstance(app_a.get()));
  ASSERT_NE(store_a, nullptr);
  ConfigureStorageEmulator(store_a.get());

  const int rounds = GetStressRounds(10);
  for (int i = 0; i < rounds; ++i) {
    std::string app_name = "multi_emu_" + std::to_string(i);
    std::unique_ptr<App> app_b(
        App::Create(MakeEmulatorAppOptions(), app_name.c_str()));
    ASSERT_NE(app_b, nullptr);

    std::unique_ptr<auth::Auth> auth_b(auth::Auth::GetAuth(app_b.get()));
    ConfigureAuthEmulator(auth_b.get());
    std::unique_ptr<storage::Storage> store_b(
        storage::Storage::GetInstance(app_b.get()));
    ConfigureStorageEmulator(store_b.get());
    std::unique_ptr<firestore::Firestore> fs_b(
        firestore::Firestore::GetInstance(app_b.get()));
    ConfigureFirestoreEmulator(fs_b.get());

    auto sign_in = auth_b->SignInAnonymously();
    ASSERT_TRUE(sign_in.Wait(15000));
    ASSERT_EQ(sign_in.error(), auth::kAuthErrorNone);

    const char payload[] = "hello-stress";
    auto upload = store_b->GetReference("stress/blob_" + std::to_string(i))
                      .PutBytes(payload, sizeof(payload) - 1);
    auto write = fs_b->Document("multi/doc_" + std::to_string(i))
                     .Set({{"i", firestore::FieldValue::Integer(i)}});
    EXPECT_TRUE(upload.Wait(15000));
    EXPECT_TRUE(write.Wait(15000));

    auth_b->SignOut();
    fs_b.reset();
    store_b.reset();
    auth_b.reset();
    app_b.reset();
  }
}

// Reproduces #1971 (Database desktop zero-delay reconnect spin loop) briefly
// and checks teardown safety under ASan/TSan.
TEST(MultiAppEmulatorStressTest, DatabaseUnreachableReconnectSpinAndTeardown) {
  std::unique_ptr<App> app(
      App::Create(MakeEmulatorAppOptions(), "rtdb_unreachable"));
  ASSERT_NE(app, nullptr);

  // Port with nothing listening; reproduces #1971 reconnect tight loop.
  std::unique_ptr<database::Database> db(database::Database::GetInstance(
      app.get(), "http://127.0.0.1:19999/?ns=demo-example"));
  ASSERT_NE(db, nullptr);

#if defined(__linux__)
  struct timespec cpu_start {};
  struct timespec cpu_end {};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
#endif

  auto set_future = db->GetReference("probe/x").SetValue(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

#if defined(__linux__)
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);
  double cpu_ms = (cpu_end.tv_sec - cpu_start.tv_sec) * 1000.0 +
                  (cpu_end.tv_nsec - cpu_start.tv_nsec) / 1e6;
  std::cout << "[PROBE #1971] 1000ms wall-clock unreachable RTDB consumed "
            << cpu_ms << " ms process CPU time" << std::endl;
#endif

  // Tear down while reconnect loop is active.
  db.reset();
  app.reset();
}

}  // namespace
}  // namespace firebase
