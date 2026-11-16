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

// A static object that owns a messaging::Listener unregisters it and
// terminates Messaging from its destructor during exit(), after main() has
// returned. Both calls must still be able to lock Messaging's listener state.
//
// The checks happen after GoogleTest has reported its result, so a regression
// shows up as the test binary crashing or aborting on exit rather than as a
// failed expectation.

#include <memory>

#include "app/src/include/firebase/app.h"
#include "gtest/gtest.h"
#include "messaging/src/include/firebase/messaging.h"

namespace firebase {
namespace messaging {
namespace {

class NoOpListener : public Listener {
 public:
  void OnMessage(const Message& /*message*/) override {}
  void OnTokenReceived(const char* /*token*/) override {}
};

// Owns the App and the listener the way an application-level singleton would.
struct ExitTimeOwner {
  ~ExitTimeOwner() {
    if (!initialized) return;
    SetListener(nullptr);
    Terminate();
  }

  std::unique_ptr<App> app;
  NoOpListener listener;
  bool initialized = false;
};

// Constructed during static initialization of this translation unit, which the
// linker places ahead of the SDK's libraries, so it is destroyed after any
// static the SDK itself registers for destruction at exit.
ExitTimeOwner g_exit_time_owner;

TEST(MessagingStaticDestructionTest, ListenerOwnedByStaticIsRemovedAfterMain) {
  AppOptions options;
  options.set_app_id("1:1234567890:android:0123456789abcdef");
  options.set_api_key("not_a_real_api_key");
  options.set_project_id("static-destruction-test");
  options.set_messaging_sender_id("1234567890");
  g_exit_time_owner.app.reset(App::Create(options));
  ASSERT_NE(g_exit_time_owner.app, nullptr);

  ASSERT_EQ(Initialize(*g_exit_time_owner.app, &g_exit_time_owner.listener),
            kInitResultSuccess);
  g_exit_time_owner.initialized = true;
}

}  // namespace
}  // namespace messaging
}  // namespace firebase
