#ifndef FIREBASE_REMOTE_CONFIG_SRC_COMMON_H_
#define FIREBASE_REMOTE_CONFIG_SRC_COMMON_H_
#include <cstddef>
namespace firebase {
namespace remote_config {
enum FutureStatus {
  kFutureStatusSuccess = 0,
  kFutureStatusFailure = 1,
};
namespace internal {
using ::firebase::remote_config::FutureStatus;
using ::firebase::remote_config::kFutureStatusFailure;
using ::firebase::remote_config::kFutureStatusSuccess;
enum RemoteConfigFn {
  kRemoteConfigFnFetch,
  kRemoteConfigFnEnsureInitialized,
  kRemoteConfigFnActivate,
  kRemoteConfigFnFetchAndActivate,
  kRemoteConfigFnSetDefaults,
  kRemoteConfigFnSetConfigSettings,
  kRemoteConfigFnSetCustomSignals,
  kRemoteConfigFnCount,
};
constexpr size_t kMaxCustomSignalsCount = 100;
constexpr size_t kMaxCustomSignalKeyLength = 256;
constexpr size_t kMaxCustomSignalValueLength = 500;
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
#endif
