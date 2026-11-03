// Copyright 2019 Google LLC
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

#include "app/src/secure/user_secure_windows_internal.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>

#define NOMINMAX
#include <wincred.h>

#include "app/src/log.h"

namespace firebase {
namespace app {
namespace secure {

namespace {

// Prefix and suffix to add to specified namespace.
constexpr char kNamespacePrefix[] = "";
constexpr char kNamespaceSuffix1[] = ".firebase.";
constexpr char kNamespaceSuffix2[] = "";
// For example:
// com.my_company.my_app.firebase.auth
// com.my_company.my_app.firebase.iid

struct CredDeleter {
  void operator()(void* ptr) const {
    if (ptr != nullptr) {
      ::CredFree(ptr);
    }
  }
};

// Returns true if an actual error occurred, false if not.
bool LogCredentialError(DWORD error, const char* func, const char* target) {
  if (error == ERROR_NOT_FOUND) {
    // If error is ERROR_NOT_FOUND, don't report an error, all is fine.
    LogDebug("%s: Credential %s not found", func, target);
    return false;
  } else if (error == ERROR_NO_SUCH_LOGON_SESSION) {
    LogWarning("%s: No logon session for credential %s", func, target);
    return true;
  } else if (error == ERROR_INVALID_FLAGS) {
    LogError("%s: Invalid flags for credential %s", func, target);
    return true;
  } else if (error == ERROR_INVALID_PARAMETER) {
    LogError("%s: Invalid parameter for credential %s", func, target);
    return true;
  } else {
    // Unknown error occurred, print it out as a warning.
    LogWarning("%s: Operation on credential %s failed with error %lu", func,
               target, static_cast<unsigned long>(error));
    return true;
  }
}

bool HasValidChunkSuffix(const char* target_name,
                         const std::string& target_prefix) {
  if (target_name == nullptr) {
    return false;
  }
  const size_t prefix_len = target_prefix.size();
  if (std::strncmp(target_name, target_prefix.c_str(), prefix_len) != 0) {
    return false;
  }
  const char* p = target_name + prefix_len;
  if (*p < '0' || *p > '9') {
    return false;
  }
  while (*p >= '0' && *p <= '9') {
    ++p;
  }
  return *p == ']' && *(p + 1) == '\0';
}

}  // namespace

UserSecureWindowsInternal::UserSecureWindowsInternal(const char* domain,
                                                     const char* key_namespace)
    : domain_(domain ? domain : "") {
  const char* safe_ns = key_namespace ? key_namespace : "";
  namespace_ = std::string(kNamespacePrefix) + safe_ns + kNamespaceSuffix1 +
               domain_ + kNamespaceSuffix2;
}

UserSecureWindowsInternal::~UserSecureWindowsInternal() = default;

std::string UserSecureWindowsInternal::GetTargetName(
    const std::string& app_name) {
  return namespace_ + "/" + app_name;
}

std::string UserSecureWindowsInternal::GetTargetName(
    const std::string& app_name, size_t idx) {
  return GetTargetName(app_name) + "[" + std::to_string(idx) + "]";
}

std::string UserSecureWindowsInternal::LoadUserData(
    const std::string& app_name) {
  std::string output;
  // Data comes in chunks, read a chunk at a time until we get a NOT_FOUND
  // error.
  for (size_t idx = 0;; ++idx) {
    std::string target = GetTargetName(app_name, idx);
    PCREDENTIALA raw_credential = nullptr;
    BOOL success =
        ::CredReadA(target.c_str(), CRED_TYPE_GENERIC, 0, &raw_credential);
    std::unique_ptr<CREDENTIALA, CredDeleter> credential(raw_credential);
    if (!success) {
      DWORD error = ::GetLastError();
      if (error == ERROR_NOT_FOUND && idx > 0) {
        // Reached the end of our data, return it.
        break;
      }
      LogCredentialError(error, "LoadUserData", target.c_str());
      return "";
    }
    if (credential->CredentialBlob != nullptr &&
        credential->CredentialBlobSize > 0) {
      output.append(reinterpret_cast<const char*>(credential->CredentialBlob),
                    static_cast<size_t>(credential->CredentialBlobSize));
    }
  }
  return output;
}

void UserSecureWindowsInternal::SaveUserData(const std::string& app_name,
                                             const std::string& user_data) {
  // First delete any existing data, so we don't have stale chunks.
  DeleteUserData(app_name);
  size_t user_data_size = user_data.length();
  size_t user_data_offset = 0;
  do {
    const char* chunk_data = user_data.c_str() + user_data_offset;
    size_t chunk_size =
        std::min(user_data_size - user_data_offset,
                 static_cast<size_t>(CRED_MAX_CREDENTIAL_BLOB_SIZE));
    size_t chunk_number = user_data_offset / CRED_MAX_CREDENTIAL_BLOB_SIZE;
    std::string target = GetTargetName(app_name, chunk_number);

    CREDENTIALA credential;
    std::memset(&credential, 0, sizeof(credential));
    credential.Flags = 0;
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPSTR>(target.c_str());
    std::string comment =
        std::string("Firebase ") + domain_ + " persistent data for " + target;
    if (comment.length() >= CRED_MAX_STRING_LENGTH) {
      comment.resize(CRED_MAX_STRING_LENGTH - 1);
    }
    credential.Comment = const_cast<LPSTR>(comment.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(chunk_size);
    credential.CredentialBlob =
        chunk_size > 0
            ? reinterpret_cast<LPBYTE>(const_cast<char*>(chunk_data))
            : nullptr;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    BOOL success = ::CredWriteA(&credential, 0);
    if (!success) {
      DWORD error = ::GetLastError();
      LogCredentialError(error, "SaveUserData", target.c_str());
      if (chunk_number > 0) {
        // Delete partially written data before returning.
        DeleteUserData(app_name);
      }
      return;
    }
    user_data_offset += CRED_MAX_CREDENTIAL_BLOB_SIZE;
  } while (user_data_offset < user_data_size);
}

void UserSecureWindowsInternal::DeleteUserData(const std::string& app_name) {
  std::string target_prefix = GetTargetName(app_name) + "[";
  std::string target_glob = target_prefix + "*";
  DWORD count = 0;
  PCREDENTIALA* raw_credentials = nullptr;
  BOOL enum_ok =
      ::CredEnumerateA(target_glob.c_str(), 0, &count, &raw_credentials);
  std::unique_ptr<PCREDENTIALA, CredDeleter> credentials(raw_credentials);
  if (enum_ok) {
    for (DWORD i = 0; i < count; ++i) {
      if (!HasValidChunkSuffix(raw_credentials[i]->TargetName, target_prefix)) {
        continue;
      }
      if (!::CredDeleteA(raw_credentials[i]->TargetName,
                         raw_credentials[i]->Type, 0)) {
        DWORD error = ::GetLastError();
        LogCredentialError(error, "DeleteUserData",
                           raw_credentials[i]->TargetName);
      }
    }
    return;
  }
  DWORD enum_error = ::GetLastError();
  if (enum_error == ERROR_NOT_FOUND) {
    return;
  }
  for (size_t idx = 0;; ++idx) {
    std::string target = GetTargetName(app_name, idx);
    BOOL success = ::CredDeleteA(target.c_str(), CRED_TYPE_GENERIC, 0);
    if (!success) {
      DWORD error = ::GetLastError();
      if (error == ERROR_NOT_FOUND) {
        break;
      }
      LogCredentialError(error, "DeleteUserData", target.c_str());
      break;
    }
  }
}

void UserSecureWindowsInternal::DeleteAllData() {
  // Enumerate all credentials and delete them.
  std::string wildcard = "*";
  std::string target_glob = GetTargetName(wildcard);
  DWORD count = 0;
  PCREDENTIALA* raw_credentials = nullptr;
  BOOL success =
      ::CredEnumerateA(target_glob.c_str(), 0, &count, &raw_credentials);
  std::unique_ptr<PCREDENTIALA, CredDeleter> credentials(raw_credentials);
  if (!success) {
    DWORD error = ::GetLastError();
    LogCredentialError(error, "DeleteAllData", target_glob.c_str());
    return;
  }
  for (DWORD i = 0; i < count; ++i) {
    success = ::CredDeleteA(raw_credentials[i]->TargetName,
                            raw_credentials[i]->Type, 0);
    if (!success) {
      DWORD error = ::GetLastError();
      LogCredentialError(error, "DeleteAllData",
                         raw_credentials[i]->TargetName);
    }
  }
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
