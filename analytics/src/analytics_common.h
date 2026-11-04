#ifndef FIREBASE_ANALYTICS_SRC_ANALYTICS_COMMON_H_
#define FIREBASE_ANALYTICS_SRC_ANALYTICS_COMMON_H_
#include <memory>
#include <mutex>
#include "app/src/include/firebase/app.h"
#include "app/src/reference_counted_future_impl.h"
namespace firebase {
namespace analytics {
namespace internal {
enum AnalyticsFn {
  kAnalyticsFnGetAnalyticsInstanceId,
  kAnalyticsFnGetSessionId,
  kAnalyticsFnLogAppleTransaction,
  kAnalyticsFnCount,
};
class FutureData {
 public:
  FutureData() : api_(kAnalyticsFnCount) {}
  ~FutureData() = default;
  FutureData(const FutureData&) = delete;
  FutureData& operator=(const FutureData&) = delete;
  FutureData(FutureData&&) = delete;
  FutureData& operator=(FutureData&&) = delete;
  ReferenceCountedFutureImpl* api() { return &api_; }
  static std::recursive_mutex& mutex();
  static FutureData* Create();
  static void Destroy();
  static FutureData* Get();
  static std::shared_ptr<FutureData> GetShared();
 private:
  static std::shared_ptr<FutureData>& FutureDataPtr();
  ReferenceCountedFutureImpl api_;
};
extern const char* const kAnalyticsModuleName;
void RegisterTerminateOnAppDestroy(const App* app);
void RegisterTerminateOnDefaultAppDestroy();
void UnregisterTerminateOnAppDestroy();
void UnregisterTerminateOnDefaultAppDestroy();
bool IsInitialized();
}  // namespace internal
}  // namespace analytics
}  // namespace firebase
#endif
