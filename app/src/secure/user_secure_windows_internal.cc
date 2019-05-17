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

#define NOMINMAX
#include <wincred.h>

namespace firebase {
namespace app {
namespace secure {

// Prefix and suffix to add to specified namespace.
static const char kNamespacePrefix[] = "";
static const char kNamespaceSuffix1[] = ".firebase.";
static const char kNamespaceSuffix2[] = "";
// For example:
// com.my_company.my_app.firebase.auth
// com.my_company.my_app.firebase.iid

UserSecureWindowsInternal::UserSecureWindowsInternal(const char* domain,
                                                     const char* key_namespace)
    : domain_(domain) {
  namespace_ = std::string(kNamespacePrefix) + key_namespace +
               kNamespaceSuffix1 + domain + kNamespaceSuffix2;
}

UserSecureWindowsInternal::~UserSecureWindowsInternal() {}

std::string UserSecureWindowsInternal::GetTargetName(
    const std::string& app_name) {
  return namespace_ + "/" + app_name;
}

std::string UserSecureWindowsInternal::GetTargetName(
    const std::string& app_name, size_t idx) {
  return GetTargetName(app_name) + "[" + std::to_string(idx) + "]";
}

// Returns true if an actual error occurred, false if not.
static bool LogCredentialError(DWORD error, const char* func,
                               const char* target) {
  if (error == ERROR_NOT_FOUND) {
    // If error is ERROR_NOT_FOUND, don't report an error, all is fine.
    LogDebug("%s: Credential %s not found", func, target);
    return false;
  } else if (error == ERROR_NO_SUCH_LOGON_SESSION) {
    LogWarning("%s: No logon session for credential %s", func, target);
    return true;
  } else if (error == ERROR_INVALID_FLAGS) {
    LogAssert("%s: Invalid flags for credential %s", func, target);
    return true;
  } else if (error == ERROR_INVALID_PARAMETER) {
    LogAssert("%s: Invalid paremeter for credential %s", func, target);
    return true;
  } else {
    // Unknown error occurred, print it out as a warning.
    LogWarning("%s: Operation on credential %s failed with error %d", func,
               target, error);
    return true;
  }
}

std::string UserSecureWindowsInternal::LoadUserData(
    const std::string& app_name) {
  std::string output;
  int idx = 0;
  // Data comes in chunks, read a chunk at a time until we get a NOT_FOUND
  // error.
  for (;; ++idx) {
    std::string target = GetTargetName(app_name, idx);
    PCREDENTIAL credential = nullptr;
    BOOL success = CredRead(target.c_str(), CRED_TYPE_GENERIC, 0, &credential);
    if (!success) {
      DWORD error = GetLastError();
      if (credential) CredFree(credential);
      if (error == ERROR_NOT_FOUND && idx > 0) {
        // Reached the end of our data, return it.
        break;
      }
      LogCredentialError(error, "LoadUserData", target.c_str());
      return "";
    }
    std::string value(reinterpret_cast<const char*>(credential->CredentialBlob),
                      static_cast<size_t>(credential->CredentialBlobSize));
    CredFree(credential);
    output = output + value;
  }
  return output;
}

void UserSecureWindowsInternal::SaveUserData(const std::string& app_name,
                                             const std::string& user_data) {
  // First delete any existing data, so we don't have stale chunks.
  DeleteUserData(app_name);
  size_t user_data_size = user_data.length();
  for (size_t user_data_offset = 0; user_data_offset < user_data_size;
       user_data_offset += CRED_MAX_CREDENTIAL_BLOB_SIZE) {
    const char* chunk_data = user_data.c_str() + user_data_offset;
    size_t chunk_size =
        std::min(user_data_size - user_data_offset,
                 static_cast<size_t>(CRED_MAX_CREDENTIAL_BLOB_SIZE));
    size_t chunk_number = user_data_offset / CRED_MAX_CREDENTIAL_BLOB_SIZE;
    std::string target = GetTargetName(app_name, chunk_number);

    CREDENTIAL credential;
    memset(&credential, 0, sizeof(credential));
    credential.Flags = 0;
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPSTR>(target.c_str());
    std::string comment =
        std::string("Firebase ") + domain_ + " persistent data for " + target;
    credential.Comment = const_cast<LPSTR>(comment.c_str());
    credential.CredentialBlobSize = chunk_size;
    credential.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char*>(chunk_data));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    BOOL success = CredWrite(&credential, 0);
    if (!success) {
      DWORD error = GetLastError();
      if (LogCredentialError(error, "SaveUserData", target.c_str()) &&
          chunk_number > 0) {
        // Delete partially written data before returning.
        DeleteUserData(app_name);
      }
      return;
    }
  }
}

void UserSecureWindowsInternal::DeleteUserData(const std::string& app_name) {
  int idx = 0;
  for (;; ++idx) {
    std::string target = GetTargetName(app_name, idx);
    BOOL success = CredDelete(target.c_str(), CRED_TYPE_GENERIC, 0);
    if (!success) {
      DWORD error = GetLastError();
      if (error == ERROR_NOT_FOUND && idx > 0) {
        // Reached the end of our data, no error.
        break;
      }
      LogCredentialError(error, "DeleteUserData", target.c_str());
      return;
    }
  }
}

void UserSecureWindowsInternal::DeleteAllData() {
  // Enumerate all credentials and delete them.
  std::string wildcard = "*";
  std::string target_glob = GetTargetName(wildcard);
  DWORD count = 0;
  PCREDENTIAL* credentials = nullptr;
  BOOL success = CredEnumerateA(target_glob.c_str(), 0, &count, &credentials);
  if (!success) {
    DWORD error = GetLastError();
    LogCredentialError(error, "DeleteAllData", target_glob.c_str());
    if (credentials) CredFree(credentials);
    return;
  }
  for (DWORD i = 0; i < count; ++i) {
    success = CredDelete(credentials[i]->TargetName, credentials[i]->Type, 0);
    if (!success) {
      DWORD error = GetLastError();
      LogCredentialError(error, "DeleteAllData", credentials[i]->TargetName);
    }
  }
  CredFree(credentials);
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
