#include "analytics/src/analytics_windows.h"
#include <wincrypt.h>
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "app/src/log.h"
#define LOG_TAG "VerifyAndLoadAnalyticsLibrary: "
namespace firebase {
namespace analytics {
namespace internal {
static std::wstring GetExecutablePath() {
  for (size_t cap : {static_cast<size_t>(MAX_PATH + 1), size_t(65537)}) {
    std::vector<wchar_t> buffer(cap);
    DWORD len =
        GetModuleFileNameW(NULL, buffer.data(), static_cast<DWORD>(cap));
    if (len == 0) {
      LogError(LOG_TAG "Failed to get executable path. Error: %u",
               GetLastError());
      return {};
    }
    if (len < cap) return std::wstring(buffer.data(), len);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      LogError(LOG_TAG "Failed to get executable path. Error: %u",
               GetLastError());
      return {};
    }
  }
  LogError(LOG_TAG "Executable path too long. Error: %u", GetLastError());
  return {};
}
static std::string CalculateFileSha256(HANDLE hFile) {
  if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    LogError(LOG_TAG "CalculateFileSha256.SetFilePointer failed. Error: %u",
             GetLastError());
    return "";
  }
  HCRYPTPROV hProv = 0;
  if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES,
                            CRYPT_VERIFYCONTEXT)) {
    LogError(LOG_TAG
             "CalculateFileSha256.CryptAcquireContextW failed. Error: %u",
             GetLastError());
    return "";
  }
  HCRYPTHASH hHash = 0;
  if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
    LogError(LOG_TAG "CalculateFileSha256.CryptCreateHash failed. Error: %u",
             GetLastError());
    CryptReleaseContext(hProv, 0);
    return "";
  }
  auto cleanup = [&]() {
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
  };
  BYTE rgbFile[1024];
  DWORD cbRead = 0;
  while (true) {
    if (!ReadFile(hFile, rgbFile, sizeof(rgbFile), &cbRead, NULL)) {
      LogError(LOG_TAG "CalculateFileSha256.ReadFile failed. Error: %u",
               GetLastError());
      cleanup();
      return "";
    }
    if (cbRead == 0) break;
    if (!CryptHashData(hHash, rgbFile, cbRead, 0)) {
      LogError(LOG_TAG "CalculateFileSha256.CryptHashData failed. Error: %u",
               GetLastError());
      cleanup();
      return "";
    }
  }
  DWORD cbHashValue = 0;
  DWORD dwCount = sizeof(DWORD);
  if (!CryptGetHashParam(hHash, HP_HASHSIZE,
                         reinterpret_cast<BYTE*>(&cbHashValue), &dwCount, 0)) {
    LogError(LOG_TAG
             "CalculateFileSha256.CryptGetHashParam (HP_HASHSIZE) failed. "
             "Error: %u",
             GetLastError());
    cleanup();
    return "";
  }
  std::vector<BYTE> binary_hash(cbHashValue);
  if (!CryptGetHashParam(hHash, HP_HASHVAL, binary_hash.data(), &cbHashValue,
                         0)) {
    LogError(LOG_TAG
             "CalculateFileSha256.CryptGetHashParam (HP_HASHVAL) failed. "
             "Error: %u",
             GetLastError());
    cleanup();
    return "";
  }
  DWORD hex_size = 0;
  if (!CryptBinaryToStringA(binary_hash.data(), binary_hash.size(),
                            CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, NULL,
                            &hex_size)) {
    LogError(LOG_TAG
             "CalculateFileSha256.CryptBinaryToStringA (size) failed. Error: "
             "%u",
             GetLastError());
    cleanup();
    return "";
  }
  std::string hex_hash(hex_size, '\0');
  if (!CryptBinaryToStringA(binary_hash.data(), binary_hash.size(),
                            CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF,
                            &hex_hash[0], &hex_size)) {
    LogError(LOG_TAG
             "CalculateFileSha256.CryptBinaryToStringA (conversion) failed. "
             "Error: %u",
             GetLastError());
    cleanup();
    return "";
  }
  hex_hash.resize(hex_size);
  cleanup();
  return hex_hash;
}
HMODULE VerifyAndLoadAnalyticsLibrary(
    const wchar_t* library_filename,
    const std::vector<std::string>& allowed_hashes) {
  if (!library_filename || library_filename[0] == L'\0') {
    LogError(LOG_TAG "Invalid arguments.");
    return nullptr;
  }
  if (allowed_hashes.empty()) {
    LogWarning(LOG_TAG "No hash provided, using unverified Analytics DLL.");
    return LoadLibraryW(library_filename);
  }
  std::wstring exe_path = GetExecutablePath();
  if (exe_path.empty()) {
    LogError(LOG_TAG "Can't determine executable path.");
    return nullptr;
  }
  size_t last_slash = exe_path.find_last_of(L"\\");
  if (last_slash == std::wstring::npos) {
    LogError(LOG_TAG "Could not determine executable directory.");
    return nullptr;
  }
  std::wstring full_dll_path =
      exe_path.substr(0, last_slash + 1) + library_filename;
  HANDLE hFile =
      CreateFileW(full_dll_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND) {
      LogError(LOG_TAG "Failed to open Analytics DLL. Error: %u", err);
    }
    return nullptr;
  }
  OVERLAPPED overlapped = {0};
  if (!LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, 0xFFFFFFFF, 0xFFFFFFFF,
                  &overlapped)) {
    LogError(LOG_TAG "Failed to lock Analytics DLL. Error: %u", GetLastError());
    CloseHandle(hFile);
    return nullptr;
  }
  HMODULE hModule = nullptr;
  std::string calculated_hash = CalculateFileSha256(hFile);
  if (calculated_hash.empty()) {
    LogError(LOG_TAG "Hash failed for Analytics DLL.");
  } else {
    bool matched = false;
    for (const auto& expected : allowed_hashes) {
      if (calculated_hash == expected) {
        matched = true;
        break;
      }
      LogDebug(LOG_TAG "Hash mismatch: got %s expected %s",
               calculated_hash.c_str(), expected.c_str());
    }
    if (matched) {
      LogDebug(LOG_TAG "Successfully verified Analytics DLL.");
      hModule = LoadLibraryW(full_dll_path.c_str());
      if (!hModule) {
        LogError(LOG_TAG "Library load failed for Analytics DLL. Error: %u",
                 GetLastError());
      }
    } else {
      LogError(LOG_TAG "Hash mismatch for Analytics DLL.");
    }
  }
  if (!UnlockFileEx(hFile, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped)) {
    LogError(LOG_TAG "Failed to unlock Analytics DLL. Error: %u",
             GetLastError());
  }
  if (!CloseHandle(hFile)) {
    LogError(LOG_TAG "Failed to close Analytics DLL. Error: %u",
             GetLastError());
  }
  return hModule;
}
}  // namespace internal
}  // namespace analytics
}  // namespace firebase
