#ifndef FIREBASE_MESSAGING_SRC_COMMON_H_
#define FIREBASE_MESSAGING_SRC_COMMON_H_
#include "app/src/reference_counted_future_impl.h"
#include "messaging/src/include/firebase/messaging.h"
namespace firebase {
namespace messaging {
namespace internal {
void RegisterTerminateOnDefaultAppDestroy();
void RegisterTerminateOnDefaultAppDestroy(const App& app);
void UnregisterTerminateOnDefaultAppDestroy();
bool IsInitialized();
}  // namespace internal
enum MessagingFn {
  kMessagingFnRequestPermission,
  kMessagingFnSubscribe,
  kMessagingFnUnsubscribe,
  kMessagingFnGetToken,
  kMessagingFnDeleteToken,
  kMessagingFnRegister,
  kMessagingFnUnregister,
  kMessagingFnCount
};
class FutureData {
 public:
  FutureData() : api_(kMessagingFnCount) {}
  ~FutureData() {}
  ReferenceCountedFutureImpl* api() { return &api_; }
  static FutureData* Create();
  static void Destroy();
  static FutureData* Get();
 private:
  ReferenceCountedFutureImpl api_;
  static FutureData* s_future_data_;
};
void NotifyListenerSet(Listener* listener);
bool HasListener();
Listener* SetListenerIfNotNull(Listener* listener);
void UnregisterListener(Listener* listener);
void NotifyListenerOnMessage(const Message& message);
void NotifyListenerOnTokenReceived(const char* token);
void NotifyListenerOnRegistrationReceived(const char* installationId);
void NotifyListenerOnUnregistrationReceived(const char* installationId);
}  // namespace messaging
}  // namespace firebase
#endif
