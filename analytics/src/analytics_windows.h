#ifndef FIREBASE_ANALYTICS_SRC_ANALYTICS_WINDOWS_H_
#define FIREBASE_ANALYTICS_SRC_ANALYTICS_WINDOWS_H_

#include <windows.h>

namespace firebase {
namespace analytics {
namespace internal {

HMODULE VerifyAndLoadAnalyticsLibrary(
    const wchar_t* library_filename, // Renamed from library_path
    const unsigned char* expected_hash,
    size_t expected_hash_size);

}  // namespace internal
}  // namespace analytics
}  // namespace firebase

#endif  // FIREBASE_ANALYTICS_SRC_ANALYTICS_WINDOWS_H_
