#ifndef FIREBASE_ANALYTICS_SRC_ANALYTICS_WINDOWS_H_
#define FIREBASE_ANALYTICS_SRC_ANALYTICS_WINDOWS_H_
#include <windows.h>
#include <string>
#include <vector>
namespace firebase {
namespace analytics {
namespace internal {
HMODULE VerifyAndLoadAnalyticsLibrary(
    const wchar_t* library_filename,
    const std::vector<std::string>& allowed_hashes);
}
}  // namespace analytics
}  // namespace firebase
#endif
