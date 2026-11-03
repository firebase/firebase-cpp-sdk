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

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "messaging/src/common.h"

namespace firebase {
namespace messaging {

DEFINE_FIREBASE_VERSION_STRING(FirebaseMessaging);

static bool g_initialized = false;
static bool g_reg_on_init_enabled = true;
static bool g_delivery_metrics_to_bigquery = false;

InitResult Initialize(const ::firebase::App& app, Listener* listener) {
  return Initialize(app, listener, MessagingOptions());
}

InitResult Initialize(const ::firebase::App& app, Listener* listener,
                      const MessagingOptions& /*options*/) {
  SetListenerIfNotNull(listener);
  if (internal::IsInitialized()) return kInitResultSuccess;
  FutureData::Create();
  g_initialized = true;
  internal::RegisterTerminateOnDefaultAppDestroy(app);
  return kInitResultSuccess;
}

namespace internal {

bool IsInitialized() { return g_initialized; }

}  // namespace internal

void Terminate() {
  if (!internal::IsInitialized()) return;
  internal::UnregisterTerminateOnDefaultAppDestroy();
  SetListener(nullptr);
  FutureData::Destroy();
  g_reg_on_init_enabled = true;
  g_delivery_metrics_to_bigquery = false;
  g_initialized = false;
}

void NotifyListenerSet(Listener* /*listener*/) {}

namespace {
// Functions to handle returning completed stub futures.

const int kStubResultCode = kErrorNone;  // Complete with no error.
const char kStubMessage[] = "Successfully completed as a stub.";
const char kStubInvalidTopicMessage[] = "Invalid topic name.";
const char kStubToken[] = "StubToken";

bool IsValidTopicName(const char* topic) {
  if (!topic || topic[0] == '\0') return false;
  const char* p = topic;
  const char kTopicsPrefix[] = "/topics/";
  const size_t kTopicsPrefixLen = sizeof(kTopicsPrefix) - 1;
  if (std::strncmp(p, kTopicsPrefix, kTopicsPrefixLen) == 0) {
    p += kTopicsPrefixLen;
  }
  size_t len = 0;
  for (; *p != '\0'; ++p, ++len) {
    const char c = *p;
    const bool valid_char =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~' || c == '%';
    if (!valid_char) return false;
  }
  return len >= 1 && len <= 900;
}

Future<void> CreateAndCompleteStubFuture(MessagingFn fn,
                                         int error = kStubResultCode,
                                         const char* msg = kStubMessage) {
  // Create a future and complete it immediately.
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(fn);
  api->Complete(handle, error, msg);
  return Future<void>(api, handle.get());
}

Future<void> GetLastResultFuture(MessagingFn fn) {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(api->LastResult(fn));
}

}  // namespace

Future<void> Subscribe(const char* topic) {
  if (!IsValidTopicName(topic)) {
    return CreateAndCompleteStubFuture(kMessagingFnSubscribe,
                                       kErrorInvalidTopicName,
                                       kStubInvalidTopicMessage);
  }
  return CreateAndCompleteStubFuture(kMessagingFnSubscribe);
}

Future<void> SubscribeLastResult() {
  return GetLastResultFuture(kMessagingFnSubscribe);
}

Future<void> Unsubscribe(const char* topic) {
  if (!IsValidTopicName(topic)) {
    return CreateAndCompleteStubFuture(kMessagingFnUnsubscribe,
                                       kErrorInvalidTopicName,
                                       kStubInvalidTopicMessage);
  }
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

bool IsRegistrationOnInitEnabled() { return g_reg_on_init_enabled; }

void SetRegistrationOnInitEnabled(bool enable) {
  g_reg_on_init_enabled = enable;
}

bool IsTokenRegistrationOnInitEnabled() { return g_reg_on_init_enabled; }

void SetTokenRegistrationOnInitEnabled(bool enable) {
  g_reg_on_init_enabled = enable;
}

bool DeliveryMetricsExportToBigQueryEnabled() {
  return g_delivery_metrics_to_bigquery;
}

void SetDeliveryMetricsExportToBigQuery(bool enable) {
  g_delivery_metrics_to_bigquery = enable;
}

Future<void> Register() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  NotifyListenerOnRegistrationReceived("StubRegistrationId");
  return CreateAndCompleteStubFuture(kMessagingFnRegister);
}

Future<void> RegisterLastResult() {
  return GetLastResultFuture(kMessagingFnRegister);
}

Future<void> Unregister() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  NotifyListenerOnUnregistrationReceived("StubRegistrationId");
  return CreateAndCompleteStubFuture(kMessagingFnUnregister);
}

Future<void> UnregisterLastResult() {
  return GetLastResultFuture(kMessagingFnUnregister);
}

Future<std::string> GetToken() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<std::string> handle =
      api->SafeAlloc<std::string>(kMessagingFnGetToken);
  api->CompleteWithResult(handle, kStubResultCode, kStubMessage,
                          std::string(kStubToken));
  return MakeFuture(api, handle);
}

Future<std::string> GetTokenLastResult() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<std::string>&>(
      api->LastResult(kMessagingFnGetToken));
}

Future<void> DeleteToken() {
  return CreateAndCompleteStubFuture(kMessagingFnDeleteToken);
}

Future<void> DeleteTokenLastResult() {
  return GetLastResultFuture(kMessagingFnDeleteToken);
}

}  // namespace messaging
}  // namespace firebase
