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
                      const MessagingOptions&) {
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
void NotifyListenerSet(Listener*) {}
namespace {
const int kStubResultCode = kErrorNone;
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
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
          c == '~' || c == '%')) {
      return false;
    }
  }
  return len >= 1 && len <= 900;
}
Future<void> CreateAndCompleteStubFuture(MessagingFn fn,
                                         int error = kStubResultCode,
                                         const char* msg = kStubMessage) {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(fn);
  api->Complete(handle, error, msg);
  return Future<void>(api, handle.get());
}
Future<void> GetLastResultFuture(MessagingFn fn) {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  return static_cast<const Future<void>&>(
      FutureData::Get()->api()->LastResult(fn));
}
}  // namespace
Future<void> Subscribe(const char* topic) {
  return IsValidTopicName(topic)
             ? CreateAndCompleteStubFuture(kMessagingFnSubscribe)
             : CreateAndCompleteStubFuture(kMessagingFnSubscribe,
                                           kErrorInvalidTopicName,
                                           kStubInvalidTopicMessage);
}
Future<void> SubscribeLastResult() {
  return GetLastResultFuture(kMessagingFnSubscribe);
}
Future<void> Unsubscribe(const char* topic) {
  return IsValidTopicName(topic)
             ? CreateAndCompleteStubFuture(kMessagingFnUnsubscribe)
             : CreateAndCompleteStubFuture(kMessagingFnUnsubscribe,
                                           kErrorInvalidTopicName,
                                           kStubInvalidTopicMessage);
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
  return static_cast<const Future<std::string>&>(
      FutureData::Get()->api()->LastResult(kMessagingFnGetToken));
}
Future<void> DeleteToken() {
  return CreateAndCompleteStubFuture(kMessagingFnDeleteToken);
}
Future<void> DeleteTokenLastResult() {
  return GetLastResultFuture(kMessagingFnDeleteToken);
}
}  // namespace messaging
}  // namespace firebase
