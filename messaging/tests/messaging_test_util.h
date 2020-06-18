// Copyright 2017 Google LLC
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

// This file contains utility methods used by messaging tests where the
// implementation diverges across platforms.
#ifndef FIREBASE_MESSAGING_CLIENT_CPP_TESTS_MESSAGING_TEST_UTIL_H_
#define FIREBASE_MESSAGING_CLIENT_CPP_TESTS_MESSAGING_TEST_UTIL_H_

namespace firebase {
namespace messaging {

struct Message;

// Sleep this thread for some amount of time and process important messages.
// e.g. let the Android messaging implementation wake up the thread watching
// the file.
void SleepMessagingTest(double seconds);

// Once-per-test platform specific initialization (e.g. the Android test
// implementation will initialize filenames by JNI calls.
void InitializeMessagingTest();

// Once-per-test platform-specific teardown.
void TerminateMessagingTest();

// Simulate a token received/refresh event from the OS-level implementation.
void OnTokenReceived(const char* tokenstr);

void OnDeletedMessages();

void OnMessageReceived(const Message& message);

void OnMessageSent(const char* message_id);

void OnMessageSentError(const char* message_id, const char* error);

}  // namespace messaging
}  // namespace firebase

#endif  // FIREBASE_MESSAGING_CLIENT_CPP_TESTS_MESSAGING_TEST_UTIL_H_
