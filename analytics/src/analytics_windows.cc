// Copyright 2025 Google LLC
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

#include "analytics/src/analytics_windows.h"

#include <wincrypt.h>
#include <windows.h>

#include <cstdlib>  // Required for _wpgmptr
#include <cstring>  // For memcmp
#include <string>   // Required for std::wstring
#include <vector>

#include "app/src/include/firebase/log.h"  //NOLINT

namespace firebase {
namespace analytics {
namespace internal {

// Helper function to calculate SHA256 hash of a file.
static std::vector<BYTE> CalculateFileSha256(HANDLE hFile) {
  HCRYPTPROV hProv = 0;
  HCRYPTHASH hHash = 0;
  std::vector<BYTE> result_hash_value;

  if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    DWORD dwError = GetLastError();
    LogError("CalculateFileSha256: SetFilePointer failed. Error: %u", dwError);
    return result_hash_value;
  }

  // Acquire Crypto Provider.
  // Using CRYPT_VERIFYCONTEXT for operations that don't require private key
  // access.
  if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES,
                            CRYPT_VERIFYCONTEXT)) {
    DWORD dwError = GetLastError();
    LogError("CalculateFileSha256: CryptAcquireContextW failed. Error: %u",
             dwError);
    return result_hash_value;
  }

  if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
    DWORD dwError = GetLastError();
    LogError("CalculateFileSha256: CryptCreateHash failed. Error: %u", dwError);
    CryptReleaseContext(hProv, 0);
    return result_hash_value;
  }

  BYTE rgbFile[1024];
  DWORD cbRead = 0;
  BOOL bReadSuccessLoop = TRUE;

  while (true) {
    bReadSuccessLoop = ReadFile(hFile, rgbFile, sizeof(rgbFile), &cbRead, NULL);
    if (!bReadSuccessLoop) {
      DWORD dwError = GetLastError();
      LogError("CalculateFileSha256: ReadFile failed. Error: %u", dwError);
      CryptDestroyHash(hHash);
      CryptReleaseContext(hProv, 0);
      return result_hash_value;
    }
    if (cbRead == 0) {
      break;
    }
    if (!CryptHashData(hHash, rgbFile, cbRead, 0)) {
      DWORD dwError = GetLastError();
      LogError("CalculateFileSha256: CryptHashData failed. Error: %u", dwError);
      CryptDestroyHash(hHash);
      CryptReleaseContext(hProv, 0);
      return result_hash_value;
    }
  }

  DWORD cbHashValue = 0;
  DWORD dwCount = sizeof(DWORD);
  if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&cbHashValue, &dwCount,
                         0)) {
    DWORD dwError = GetLastError();
    LogError(
        "CalculateFileSha256: CryptGetHashParam (HP_HASHSIZE) failed. Error: "
        "%u",
        dwError);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return result_hash_value;
  }

  result_hash_value.resize(cbHashValue);
  if (!CryptGetHashParam(hHash, HP_HASHVAL, result_hash_value.data(),
                         &cbHashValue, 0)) {
    DWORD dwError = GetLastError();
    LogError(
        "CalculateFileSha256: CryptGetHashParam (HP_HASHVAL) failed. Error: %u",
        dwError);
    result_hash_value.clear();
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return result_hash_value;
  }

  CryptDestroyHash(hHash);
  CryptReleaseContext(hProv, 0);
  return result_hash_value;
}

HMODULE VerifyAndLoadAnalyticsLibrary(
    const wchar_t* library_filename,  // This is expected to be just the DLL
                                      // filename e.g. "analytics_win.dll"
    const unsigned char* expected_hash, size_t expected_hash_size) {
  if (library_filename == nullptr || library_filename[0] == L'\0') {
    LogError("VerifyAndLoadAnalyticsLibrary: Invalid arguments.");
    return nullptr;
  }
  if (expected_hash == nullptr || expected_hash_size == 0) {
    // Don't check the hash, just load the library.
    return LoadLibraryExW(library_filename, NULL,
                          LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
  }

  // Get full path to the executable using _wpgmptr.
  // This global variable is provided by the CRT and is expected to be available
  // on Windows.
  if (_wpgmptr == nullptr || _wpgmptr[0] == L'\0') {
    LogError("VerifyAndLoadAnalyticsLibrary: Can't determine executable path.");
    return nullptr;
  }
  std::wstring executable_path_str(_wpgmptr);

  size_t last_slash_pos = executable_path_str.find_last_of(L"\\");
  if (last_slash_pos == std::wstring::npos) {
    LogError(
        "VerifyAndLoadAnalyticsLibrary: Can't determine executable directory.");
    return nullptr;
  }

  std::wstring full_dll_path_str =
      executable_path_str.substr(0, last_slash_pos + 1);
  full_dll_path_str += library_filename;  // library_filename is the filename

  HANDLE hFile =
      CreateFileW(full_dll_path_str.c_str(), GENERIC_READ, FILE_SHARE_READ,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    DWORD dwError = GetLastError();
    // If the DLL is simply not found, silently proceed to stub mode without
    // logging an error. For other errors (e.g., access denied on an existing
    // file), log them as it's an unexpected issue.
    if (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND) {
      LogError(
          "VerifyAndLoadAnalyticsLibrary: Failed to open Analytics DLL. Error: "
          "%u",
          dwError);
    }
    return nullptr;  // In all CreateFileW failure cases, return nullptr to fall
                     // back to stub mode.
  }

  OVERLAPPED overlapped = {0};
  // Attempt to lock the entire file exclusively (LOCKFILE_EXCLUSIVE_LOCK).
  // This helps ensure no other process modifies the file while we are verifying
  // and loading it.
  BOOL bFileLocked = LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, 0xFFFFFFFF,
                                0xFFFFFFFF, &overlapped);
  if (!bFileLocked) {
    DWORD dwError = GetLastError();
    LogError(
        "VerifyAndLoadAnalyticsLibrary: Failed to lock Analytics DLL. Error: "
        "%u",
        dwError);
    CloseHandle(hFile);
    return nullptr;
  }

  HMODULE hModule = nullptr;

  std::vector<BYTE> calculated_hash = CalculateFileSha256(hFile);

  if (calculated_hash.empty()) {
    LogError("VerifyAndLoadAnalyticsLibrary: Hash failed for Analytics DLL.");
  } else {
    if (calculated_hash.size() != expected_hash_size) {
      LogError(
          "VerifyAndLoadAnalyticsLibrary: Hash size mismatch for Analytics "
          "DLL. Expected: %zu, Calculated: %zu.",
          expected_hash_size, calculated_hash.size());
    } else if (memcmp(calculated_hash.data(), expected_hash,
                      expected_hash_size) != 0) {
      LogError(
          "VerifyAndLoadAnalyticsLibrary: Hash mismatch for Analytics DLL.");
    } else {
      // Load the library. LOAD_LIBRARY_SEARCH_APPLICATION_DIR is a security
      // measure to help ensure that the DLL is loaded from the application's
      // installation directory, mitigating risks of DLL preloading attacks from
      // other locations. Crucially, LoadLibraryExW with this flag needs the DLL
      // *filename only* (library_filename), not the full path we constructed
      // for CreateFileW.
      hModule = LoadLibraryExW(library_filename, NULL,
                               LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
      if (hModule == NULL) {
        DWORD dwError = GetLastError();
        LogError(
            "VerifyAndLoadAnalyticsLibrary: Library load failed for Analytics "
            "DLL. Error: %u",
            dwError);
      }
    }
  }

  if (bFileLocked) {
    if (!UnlockFileEx(hFile, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped)) {
      DWORD dwError = GetLastError();
      LogError(
          "VerifyAndLoadAnalyticsLibrary: Failed to unlock Analytics DLL. "
          "Error: %u",
          dwError);
    }
  }

  if (hFile != INVALID_HANDLE_VALUE) {
    if (!CloseHandle(hFile)) {
      DWORD dwError = GetLastError();
      LogError(
          "VerifyAndLoadAnalyticsLibrary: Failed to close Analytics DLL. "
          "Error: %u",
          dwError);
    }
  }
  return hModule;
}

}  // namespace internal
}  // namespace analytics
}  // namespace firebase
