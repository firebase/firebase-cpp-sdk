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

#include "app/src/secure/user_secure_linux_internal.h"

#include <dlfcn.h>

#include <iostream>

#include "app/src/log.h"
#include "app/src/secure/user_secure_data_handle.h"

namespace firebase {
namespace app {
namespace secure {

namespace {
// key entry for the app name in the schema. When save the user data with a
// given app name, the app name is the attribute of this key inside schema.
const char kAppNameKey[] = "firebase_app_name";
// A common attribute-value pair is added to all the device keys. This makes it
// possible to match all the keys easily (and remove them all at once).
const char kStorageDomainKey[] = "user_secure_domain";

SecretSchema BuildSchema(const char key_namespace[]) {
  SecretSchema schema = {
      key_namespace,
      SECRET_SCHEMA_NONE,
      {
          {kAppNameKey, SECRET_SCHEMA_ATTRIBUTE_STRING},
          {kStorageDomainKey, SECRET_SCHEMA_ATTRIBUTE_STRING},
      }};
  return schema;
}
}  // namespace

UserSecureLinuxInternal::UserSecureLinuxInternal(const char* domain,
                                                 const char* key_namespace)
    : domain_(domain), key_namespace_(key_namespace), known_error_code_(0) {
  storage_schema_ = BuildSchema(key_namespace_.c_str());
}

UserSecureLinuxInternal::~UserSecureLinuxInternal() {}

std::string UserSecureLinuxInternal::LoadUserData(const std::string& app_name) {
  std::string empty_str("");

  if (key_namespace_.length() <= 0) {
    return empty_str;
  }

  GError* error = nullptr;
  char* result = secret_password_lookup_sync(
      &storage_schema_,
      /* cancellable= */ nullptr,
      /* error= */ &error,
      /* key1= */ kAppNameKey,
      /* value1= */ app_name.c_str(), /* key2= */ kStorageDomainKey,
      /* value2= */ domain_.c_str(), nullptr);

  if (CheckForError(&error, "lookup") || result == nullptr) {
    return empty_str;
  }
  std::string str_result(result);
  secret_password_free(result);

  return str_result;
}

void UserSecureLinuxInternal::SaveUserData(const std::string& app_name,
                                           const std::string& user_data) {
  if (key_namespace_.length() <= 0) {
    return;
  }
  GError* error = nullptr;
  secret_password_store_sync(
      &storage_schema_, SECRET_COLLECTION_DEFAULT, /* label= */ "UserSecure",
      /* password= */ user_data.c_str(), /* cancellable= */ nullptr,
      /* error= */ &error, /* key1= */ kAppNameKey,
      /* value1= */ app_name.c_str(),
      /* key2= */ kStorageDomainKey, /* value2= */ domain_.c_str(), nullptr);

  CheckForError(&error, "store");
}

void UserSecureLinuxInternal::DeleteUserData(const std::string& app_name) {
  if (key_namespace_.length() <= 0) {
    return;
  }
  GError* error = nullptr;
  secret_password_clear_sync(&storage_schema_,
                             /* cancellable= */ nullptr, /* error= */ &error,
                             /* key1= */ kAppNameKey,
                             /* value1= */ app_name.c_str(),
                             /* key2= */ kStorageDomainKey,
                             /* value2= */ domain_.c_str(), nullptr);

  CheckForError(&error, "clear");
}

void UserSecureLinuxInternal::DeleteAllData() {
  if (key_namespace_.length() <= 0) {
    return;
  }
  GError* error = nullptr;
  secret_password_clear_sync(&storage_schema_, /* cancellable= */ nullptr,
                             /* error= */ &error,
                             /* key2= */ kStorageDomainKey,
                             /* value2= */ domain_.c_str(), nullptr);
  CheckForError(&error, "clear");
}

bool UserSecureLinuxInternal::CheckForError(GError** error,
                                            const char* function_name) {
  if (error == nullptr || *error == nullptr) return false;
  if ((*error)->code != known_error_code_) {
    LogWarning("Secret %s failed. Error %d: %s", function_name, (*error)->code,
               (*error)->message);
    known_error_code_ = (*error)->code;
  }
  g_error_free(*error);
  *error = nullptr;
  return true;
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
