// Copyright 2016 Google LLC
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

#include "messaging/src/include/firebase/messaging.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "messaging/src/common.h"

#ifdef _WIN32
// pthreads not available on Windows, so disable the fake implementation.
#define FIREBASE_MESSAGING_TEST_ENABLED 0
#else
#define FIREBASE_MESSAGING_TEST_ENABLED 0
#endif  // _WIN32

#if FIREBASE_MESSAGING_TEST_ENABLED
#include <pthread.h>
#include <unistd.h>

#include <sstream>

#include "app/src/mutex.h"
#endif  // FIREBASE_MESSAGING_TEST_ENABLED

namespace firebase {
namespace messaging {

DEFINE_FIREBASE_VERSION_STRING(FirebaseMessaging);

static bool g_initialized = false;

#if FIREBASE_MESSAGING_TEST_ENABLED
static pthread_t g_message_thread;
static Mutex g_message_thread_mutex;  // NOLINT
static bool g_message_thread_run;
static const int kMessageIntervalMilliseconds = 5000;
#endif  // FIREBASE_MESSAGING_TEST_ENABLED

#if FIREBASE_MESSAGING_TEST_ENABLED
// Thread which sends a message every few seconds.
static void* MessageTestThread(void* arg) {
  static const int kPollIntervalMilliseconds = 100;
  static const int kMicrosecondsPerMillisecond = 1000;
  int time_elapsed_milliseconds = 0;
  for (;;) {
    {
      MutexLock lock(g_message_thread_mutex);
      if (!g_message_thread_run) break;
    }
    // Wait until kMessageIntervalMilliseconds has elapsed.
    usleep(kPollIntervalMilliseconds * kMicrosecondsPerMillisecond);
    time_elapsed_milliseconds += kPollIntervalMilliseconds;
    if (time_elapsed_milliseconds < kMessageIntervalMilliseconds) continue;
    time_elapsed_milliseconds = 0;

    // Send a message.
    time_t seconds_since_epoch = time(nullptr);
    std::stringstream ss;
    ss << static_cast<int64_t>(seconds_since_epoch);
    Notification* notification = new Notification;
    notification->title = "Testing testing 1 2 3...";
    notification->body = "Hi, this is just a test.";
    notification->body += " " + ss.str();
    Message message;
    message.from = "test";
    message.to = "you";
    message.data["this"] = "is";
    message.data["a"] = "test";
    ss << reinterpret_cast<intptr_t>(notification);
    message.message_id = ss.str();
    message.notification = notification;
    NotifyListenerOnMessage(message);
  }
  return arg;
}
#endif  // FIREBASE_MESSAGING_TEST_ENABLED

InitResult Initialize(const ::firebase::App& app, Listener* listener) {
  return Initialize(app, listener, MessagingOptions());
}

InitResult Initialize(const ::firebase::App& /*app*/, Listener* listener,
                      const MessagingOptions& /*options*/) {
  if (internal::IsInitialized()) return kInitResultSuccess;
  SetListenerIfNotNull(listener);
  FutureData::Create();
#if FIREBASE_MESSAGING_TEST_ENABLED
  {
    MutexLock lock(g_message_thread_mutex);
    if (g_message_thread_run) return kInitResultSuccess;
    g_message_thread_run = true;
    pthread_attr_t attr;
    int ret = pthread_attr_init(&attr);
    FIREBASE_ASSERT(ret == 0);
    ret = pthread_create(&g_message_thread, &attr, MessageTestThread, nullptr);
    FIREBASE_ASSERT(ret == 0);
  }
#endif  // FIREBASE_MESSAGING_TEST_ENABLED
  g_initialized = true;
  internal::RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

namespace internal {

bool IsInitialized() { return g_initialized; }

}  // namespace internal

void Terminate() {
  if (!internal::IsInitialized()) return;
  internal::UnregisterTerminateOnDefaultAppDestroy();
#if FIREBASE_MESSAGING_TEST_ENABLED
  {
    MutexLock lock(g_message_thread_mutex);
    if (!g_message_thread_run) return;
    g_message_thread_run = false;
  }
  int ret = pthread_join(g_message_thread, nullptr);
  FIREBASE_ASSERT(ret == 0);
#endif  // FIREBASE_MESSAGING_TEST_ENABLED
  FutureData::Destroy();
  g_initialized = false;
}

void NotifyListenerSet(Listener* /*listener*/) {}

namespace {
// Functions to handle returning completed stub futures.

const int kStubResultCode = 0;  // Complete with no error.
const char kStubMessage[] = "Successfully completed as a stub.";
const char kStubToken[] = "StubToken";

Future<void> CreateAndCompleteStubFuture(MessagingFn fn) {
  // Create a future and complete it immediately.
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(fn);
  api->Complete(handle, kStubResultCode, kStubMessage);
  return Future<void>(api, handle.get());
}

Future<void> GetLastResultFuture(MessagingFn fn) {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(api->LastResult(fn));
}

}  // namespace

Future<void> Subscribe(const char* /*topic*/) {
  return CreateAndCompleteStubFuture(kMessagingFnSubscribe);
}

Future<void> SubscribeLastResult() {
  return GetLastResultFuture(kMessagingFnSubscribe);
}

Future<void> Unsubscribe(const char* /*topic*/) {
  return CreateAndCompleteStubFuture(kMessagingFnUnsubscribe);
}

Future<void> UnsubscribeLastResult() {
  return GetLastResultFuture(kMessagingFnUnsubscribe);
}

Future<void> RequestPermission() {
  return CreateAndCompleteStubFuture(kMessagingFnRequestPermission);
}

Future<void> RequestPermissionLastResult() {
  return GetLastResultFuture(kMessagingFnRequestPermission);
}

bool IsTokenRegistrationOnInitEnabled() { return true; }

void SetTokenRegistrationOnInitEnabled(bool /*enable*/) {}

bool DeliveryMetricsExportToBigQueryEnabled() { return false; }

void SetDeliveryMetricsExportToBigQuery(bool /*enable*/) {}

Future<std::string> GetToken() {
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<std::string> handle =
      api->SafeAlloc<std::string>(kMessagingFnGetToken);
  api->CompleteWithResult(handle, kStubResultCode, kStubMessage,
                          std::string(kStubToken));
  return MakeFuture(api, handle);
}

Future<std::string> GetTokenLastResult() {
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<std::string>&>(
      api->LastResult(kMessagingFnGetToken));
}

Future<void> DeleteToken() {
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnDeleteToken);
  api->Complete(handle, kStubResultCode, kStubMessage);
  return MakeFuture(api, handle);
}

Future<void> DeleteTokenLastResult() {
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnDeleteToken));
}

}  // namespace messaging
}  // namespace firebase
