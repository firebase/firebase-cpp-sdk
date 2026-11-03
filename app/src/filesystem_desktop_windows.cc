// Copyright 2017 Google LLC
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

#include "app/src/filesystem.h"

#include <shlobj.h>
#include <stringapiset.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "app/src/util.h"

namespace firebase {

namespace {

std::wstring Utf8ToNative(const std::string& input, std::string* out_error) {
  int input_len = static_cast<int>(input.size());

  // `MultiByteToWideChar` considers a zero length to be an error, so
  // special-case the empty string.
  if (input_len == 0) {
    return L"";
  }

  auto conversion_func = [&](wchar_t* output, int output_size) {
    // The input buffer may contain embedded nulls and isn't necessarily null-
    // terminated so we must pass an explicit length to `MultiByteToWideChar`.
    // This means the result is the number of wchar_t required to hold the
    // result, excluding the null terminator.
    return ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                                 input_len, output, output_size);
  };

  int output_len = conversion_func(nullptr, 0);
  if (output_len <= 0) {
    // TODO(varconst): output the human-readable error description as well (see
    // `GetLastError` in Firestore).
    DWORD error = ::GetLastError();
    if (out_error) {
      *out_error = "Utf8ToNative failed with code " + std::to_string(error);
    }

    return L"";
  }

  int output_terminated_len = output_len + 1;
  std::wstring output(output_terminated_len, L'\0');
  int converted_len = conversion_func(&output[0], output_len);
  if (converted_len <= 0 || converted_len >= output_terminated_len ||
      output[output_len] != L'\0') {
    if (out_error) {
      *out_error = "Utf8ToNative failed: MultiByteToWideChar returned " +
                   std::to_string(converted_len);
    }

    return L"";
  }

  output.resize(converted_len);
  return output;
}

std::string NativeToUtf8(const wchar_t* input, size_t input_size,
                         std::string* out_error) {
  int input_len = static_cast<int>(input_size);
  if (input_len == 0) {
    return "";
  }

  auto conversion_func = [&](char* output, int output_size) {
    // The input buffer may contain embedded nulls and isn't necessarily null-
    // terminated so we must pass an explicit length to `WideCharToMultiByte`.
    // This means the result is the number of char required to hold the result,
    // excluding the null terminator.
    return ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input,
                                 input_len, output, output_size, nullptr,
                                 nullptr);
  };

  int output_len = conversion_func(nullptr, 0);
  if (output_len <= 0) {
    if (out_error) {
      DWORD error = ::GetLastError();
      // TODO(varconst): output the human-readable error description as well
      // (see `GetLastError` in Firestore).
      *out_error = "NativeToUtf8 failed with code " + std::to_string(error);
    }

    return "";
  }

  int output_terminated_len = output_len + 1;
  std::string output(output_terminated_len, '\0');

  int converted_len = conversion_func(&output[0], output_len);
  if (converted_len <= 0 || converted_len >= output_terminated_len ||
      output[output_len] != '\0') {
    if (out_error) {
      *out_error = "NativeToUtf8 failed: WideCharToMultiByte returned " +
                   std::to_string(converted_len);
    }

    return "";
  }

  output.resize(converted_len);
  return output;
}

std::string NativeToUtf8(const std::wstring& input, std::string* out_error) {
  return NativeToUtf8(input.c_str(), input.size(), out_error);
}

bool Mkdir(const std::wstring& path, std::string* out_error) {
  if (::CreateDirectoryW(path.c_str(), nullptr)) {
    return true;
  }

  DWORD error = ::GetLastError();
  if (error == ERROR_ALREADY_EXISTS) {
    // POSIX returns ENOTDIR if the path exists but isn't a directory. Win32
    // doesn't make this distinction, so figure this out after the fact.
    DWORD attrs = ::GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
      error = ::GetLastError();
      if (out_error) {
        *out_error = "Could not create directory " +
                     NativeToUtf8(path, nullptr) +
                     " (error code: " + std::to_string(error) + ")";
      }

      return false;

    } else if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
      return true;
    }

    if (out_error) {
      *out_error = "Could not create directory " + NativeToUtf8(path, nullptr) +
                   ": non-directory already exists";
    }

    return false;
  }

  if (out_error) {
    *out_error = std::string("Could not create directory ") +
                 NativeToUtf8(path, nullptr) +
                 " (error code: " + std::to_string(error) + ")";
  }

  return false;
}

}  // namespace

std::string AppDataDir(const char* app_name, bool should_create,
                       std::string* out_error) {
  if (!app_name || std::strlen(app_name) == 0) {
    if (out_error) {
      *out_error = "AppDataDir failed: no app_name provided";
    }
    return "";
  }

  if (std::string(app_name).find('\\') != std::string::npos) {
    if (out_error) {
      *out_error =
          "AppDataDir failed: app_name should not contain backward slashes";
    }
    return "";
  }

  wchar_t* raw_path = nullptr;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw_path);
  std::unique_ptr<wchar_t, decltype(&::CoTaskMemFree)> path_guard(
      raw_path, &::CoTaskMemFree);
  if (FAILED(hr) || raw_path == nullptr) {
    if (out_error) {
      *out_error =
          "Failed to find the local application data directory (error code: " +
          std::to_string(hr) + ")";
    }

    return "";
  }

  std::wstring base_dir(raw_path, wcslen(raw_path));
  if (base_dir.empty()) {
    if (out_error) {
      *out_error =
          "Failed to find the local application data directory: empty path";
    }
    return "";
  }

  std::wstring current_path = base_dir;
  bool has_segment = false;
  // App name might contain path separators. Split it to get list of subdirs.
  for (const std::string& nested_dir : SplitString(app_name, '/')) {
    if (nested_dir.empty()) {
      continue;
    }
    has_segment = true;
    std::wstring nested_dir_utf16 = Utf8ToNative(nested_dir, out_error);
    if (nested_dir_utf16.empty()) {
      return "";
    }
    current_path += L"/";
    current_path += nested_dir_utf16;
    if (should_create) {
      bool created = Mkdir(current_path, out_error);
      if (!created) return "";
    }
  }

  if (!has_segment) {
    if (out_error) {
      *out_error = "AppDataDir failed: no valid app_name segments provided";
    }
    return "";
  }

  return NativeToUtf8(current_path, out_error);
}

}  // namespace firebase
