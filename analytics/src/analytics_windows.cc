#include "analytics_windows.h"

#include <windows.h>
#include <wincrypt.h>
#include <vector>
#include <string> // Required for std::wstring
#include <cstdlib> // Required for _wpgmptr
#include <cstring> // For memcmp
#include "app/src/log.h" //NOLINT

namespace firebase {
namespace analytics {
namespace internal {

// Helper function to retrieve the full path of the current executable.
// Returns an empty string on failure.
static std::wstring GetExecutablePath() {
    std::wstring executable_path_str;
    wchar_t* wpgmptr_val = nullptr;

    // Prefer _get_wpgmptr()
    errno_t err_w = _get_wpgmptr(&wpgmptr_val);
    if (err_w == 0 && wpgmptr_val != nullptr && wpgmptr_val[0] != L'\0') {
        executable_path_str = wpgmptr_val;
    } else {
        // Fallback to _get_pgmptr() and convert to wide string
        char* pgmptr_val = nullptr;
        errno_t err_c = _get_pgmptr(&pgmptr_val);
        if (err_c == 0 && pgmptr_val != nullptr && pgmptr_val[0] != '\0') {
            // Convert narrow string to wide string using CP_ACP (system default ANSI code page)
            int wide_char_count = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, pgmptr_val, -1, NULL, 0);
            if (wide_char_count == 0) { // Failure if count is 0
                DWORD conversion_error = GetLastError();
                LogError("GetExecutablePath: MultiByteToWideChar failed to calculate size for _get_pgmptr path. Error: %u", conversion_error);
                return L"";
            }

            std::vector<wchar_t> wide_path_buffer(wide_char_count);
            if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, pgmptr_val, -1, wide_path_buffer.data(), wide_char_count) == 0) {
                DWORD conversion_error = GetLastError();
                LogError("GetExecutablePath: MultiByteToWideChar failed to convert _get_pgmptr path. Error: %u", conversion_error);
                return L"";
            }
            executable_path_str = wide_path_buffer.data();
        } else {
            // Both _get_wpgmptr and _get_pgmptr failed or returned empty/null
            LogError("GetExecutablePath: Failed to retrieve executable path using both _get_wpgmptr (err: %d) and _get_pgmptr (err: %d).", err_w, err_c);
            return L"";
        }
    }

    // Safeguard if path somehow resolved to empty despite initial checks.
    if (executable_path_str.empty()) {
        LogError("GetExecutablePath: Executable path resolved to an empty string.");
        return L"";
    }
    return executable_path_str;
}

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
    // Using CRYPT_VERIFYCONTEXT for operations that don't require private key access.
    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        DWORD dwError = GetLastError();
        LogError("CalculateFileSha256: CryptAcquireContextW failed. Error: %u", dwError);
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
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&cbHashValue, &dwCount, 0)) {
        DWORD dwError = GetLastError();
        LogError("CalculateFileSha256: CryptGetHashParam (HP_HASHSIZE) failed. Error: %u", dwError);
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return result_hash_value;
    }

    result_hash_value.resize(cbHashValue);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, result_hash_value.data(), &cbHashValue, 0)) {
        DWORD dwError = GetLastError();
        LogError("CalculateFileSha256: CryptGetHashParam (HP_HASHVAL) failed. Error: %u", dwError);
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
    const wchar_t* library_filename, // This is expected to be just the DLL filename e.g. "analytics_win.dll"
    const unsigned char* expected_hash,
    size_t expected_hash_size) {

    if (library_filename == nullptr || library_filename[0] == L'\0' ||
        expected_hash == nullptr || expected_hash_size == 0) {
        LogError("VerifyAndLoadAnalyticsLibrary: Invalid arguments provided. Library path or hash details are missing.");
        return nullptr;
    }

    std::wstring executable_path_str = GetExecutablePath();

    if (executable_path_str.empty()) {
        // GetExecutablePath() is expected to log specific errors.
        // This log indicates the failure to proceed within this function.
        LogError("VerifyAndLoadAnalyticsLibrary: Failed to determine executable path via GetExecutablePath(), cannot proceed.");
        return nullptr;
    }

    size_t last_slash_pos = executable_path_str.find_last_of(L"\\");
    if (last_slash_pos == std::wstring::npos) {
        // Log message updated to avoid using %ls for executable_path_str
        LogError("VerifyAndLoadAnalyticsLibrary: Could not determine executable directory from retrieved path (no backslash found).");
        return nullptr;
    }

    std::wstring full_dll_path_str = executable_path_str.substr(0, last_slash_pos + 1);
    full_dll_path_str += library_filename; // library_filename is the filename

    HANDLE hFile = CreateFileW(full_dll_path_str.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD dwError = GetLastError();
        // If the DLL is simply not found, silently proceed to stub mode without logging an error.
        // For other errors (e.g., access denied on an existing file), log them as it's an unexpected issue.
        if (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND) {
            LogError("VerifyAndLoadAnalyticsLibrary: Failed to open the Analytics DLL (it may be present but inaccessible). Error: %u", dwError);
        }
        return nullptr; // In all CreateFileW failure cases, return nullptr to fall back to stub mode.
    }

    OVERLAPPED overlapped = {0};
    // Attempt to lock the entire file exclusively (LOCKFILE_EXCLUSIVE_LOCK).
    // This helps ensure no other process modifies the file while we are verifying and loading it.
    BOOL bFileLocked = LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped);
    if (!bFileLocked) {
        DWORD dwError = GetLastError();
        LogError("VerifyAndLoadAnalyticsLibrary: LockFileEx failed for the DLL. Error: %u", dwError);
        CloseHandle(hFile);
        return nullptr;
    }

    HMODULE hModule = nullptr;

    std::vector<BYTE> calculated_hash = CalculateFileSha256(hFile);

    if (calculated_hash.empty()) {
        LogError("VerifyAndLoadAnalyticsLibrary: SHA256 hash calculation failed for the DLL.");
    } else {
        if (calculated_hash.size() != expected_hash_size) {
            LogError("VerifyAndLoadAnalyticsLibrary: Hash size mismatch for DLL. Expected: %zu, Calculated: %zu.",
                     expected_hash_size, calculated_hash.size());
        } else if (memcmp(calculated_hash.data(), expected_hash, expected_hash_size) != 0) {
            LogError("VerifyAndLoadAnalyticsLibrary: SHA256 hash mismatch for the DLL.");
        } else {
            // Load the library. LOAD_LIBRARY_SEARCH_APPLICATION_DIR is a security measure
            // to help ensure that the DLL is loaded from the application's installation directory,
            // mitigating risks of DLL preloading attacks from other locations.
            // Crucially, LoadLibraryExW with this flag needs the DLL *filename only* (library_filename),
            // not the full path we constructed for CreateFileW.
            hModule = LoadLibraryExW(library_filename, NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
            if (hModule == NULL) {
                DWORD dwError = GetLastError();
                LogError("VerifyAndLoadAnalyticsLibrary: LoadLibraryExW failed for the DLL after hash verification. Error: %u", dwError);
            } else {
                LogInfo("VerifyAndLoadAnalyticsLibrary: DLL loaded successfully at address %p.", hModule);
            }
        }
    }

    if (bFileLocked) {
        if (!UnlockFileEx(hFile, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped)) {
            DWORD dwError = GetLastError();
            LogError("VerifyAndLoadAnalyticsLibrary: UnlockFileEx failed for the DLL. Error: %u", dwError);
        }
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(hFile)) {
            DWORD dwError = GetLastError();
            LogError("VerifyAndLoadAnalyticsLibrary: CloseHandle failed for the DLL. Error: %u", dwError);
        }
    }
    return hModule;
}

}  // namespace internal
}  // namespace analytics
}  // namespace firebase
