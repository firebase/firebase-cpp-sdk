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

#include "auth/src/desktop/secure/user_secure_linux_internal.h"

#include <dlfcn.h>

#include <iostream>

#include "auth/src/desktop/secure/user_secure_data_handle.h"

namespace firebase {
namespace auth {
namespace secure {

namespace {

// key entry for the app name in the schema. When save the user data with a
// given app name, the app name is the attribute of this key inside schema.
const char kAppNameKey[] = "auth_app_name";
// A common attribute-value pair is added to all the device keys. This makes it
// possible to match all the keys easily (and remove them all at once).
const char kCommonKeyId[] = "common_key_id";
const char kCommonKeyValue[] = "common_key_value";
// Helper function to build the right schema using the provided namespace, for
// storing keys. For instance, helps create separate storage schema for storing
// actual vs testing keys
const char kAuthKeyName[] = "com.google.firebase.auth.Keys";
SecretSchema BuildSchema(const char key_namespace[]) {
  SecretSchema schema = {key_namespace,
                         SECRET_SCHEMA_NONE,
                         {
                             {kAppNameKey, SECRET_SCHEMA_ATTRIBUTE_STRING},
                             {kCommonKeyId, SECRET_SCHEMA_ATTRIBUTE_STRING},
                         }};
  return schema;
}

}  // namespace

UserSecureLinuxInternal::UserSecureLinuxInternal()
    : UserSecureLinuxInternal(kAuthKeyName) {}

UserSecureLinuxInternal::UserSecureLinuxInternal(const char key_namespace[])
    : storage_schema_(BuildSchema(key_namespace)) {}

UserSecureLinuxInternal::~UserSecureLinuxInternal() {}

std::string UserSecureLinuxInternal::LoadUserData(const std::string appName) {
  fprintf(stderr, "internal loading start\n");
  std::string empty_str("");
  GError* error = nullptr;
  char* result =
      secret_password_lookup_sync(&storage_schema_,
                                  /* cancellable= */ nullptr,
                                  /* error= */ &error,
                                  /* key1= */ kAppNameKey,
                                  /* value1= */ appName.c_str(), nullptr);
  if (error) {
    g_error_free(error);
    return empty_str;
  }

  if (result == nullptr) {
    return empty_str;
  }
  std::string str_result(result);
  secret_password_free(result);

  return str_result;
}

void UserSecureLinuxInternal::SaveUserData(const std::string appName,
                                           const std::string userData) {
  secret_password_store_sync(
      &storage_schema_, SECRET_COLLECTION_DEFAULT, /* label= */ "UserSecure",
      /* password= */ userData.c_str(), /* cancellable= */ nullptr,
      /* error= */ nullptr, /* key1= */ kAppNameKey,
      /* value1= */ appName.c_str(),
      /* key2= */ kCommonKeyId, /* value2= */ kCommonKeyValue, nullptr);
}

void UserSecureLinuxInternal::DeleteUserData(const std::string appName) {
  secret_password_clear_sync(&storage_schema_,
                             /* cancellable= */ nullptr, /* error= */ nullptr,
                             /* key1= */ kAppNameKey,
                             /* value1= */ appName.c_str(), nullptr);
}

void UserSecureLinuxInternal::DeleteAllData() {
  secret_password_clear_sync(&storage_schema_, /* cancellable= */ nullptr,
                             /* error= */ nullptr, /* key2= */ kCommonKeyId,
                             /* value2= */ kCommonKeyValue, nullptr);
}

}  // namespace secure
}  // namespace auth
}  // namespace firebase
