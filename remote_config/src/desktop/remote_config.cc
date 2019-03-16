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

#include "remote_config/src/include/firebase/remote_config.h"

#include "firebase/app.h"
#include "firebase/future.h"
#include "remote_config/src/common.h"
#include "remote_config/src/desktop/file_manager.h"
#include "remote_config/src/desktop/remote_config_desktop.h"

#ifndef SWIG
#include "firebase/variant.h"
#endif  // SWIG

namespace firebase {
namespace remote_config {

static const char* kFilePath = "remote_config_data";
static internal::RemoteConfigDesktop* g_remote_config_desktop_instance =
    nullptr;
static internal::RemoteConfigFileManager* g_file_manager = nullptr;

namespace internal {

bool IsInitialized() { return g_remote_config_desktop_instance; }

}  // namespace internal

InitResult Initialize(const App& app) {
  if (internal::IsInitialized()) return kInitResultSuccess;
  if (g_remote_config_desktop_instance == nullptr) {
    if (g_file_manager == nullptr) {
      g_file_manager = new internal::RemoteConfigFileManager(kFilePath);
    }
    FutureData::Create();
    g_remote_config_desktop_instance =
        new internal::RemoteConfigDesktop(app, *g_file_manager);
  }
  internal::RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

void Terminate() {
  if (!internal::IsInitialized()) return;
  internal::UnregisterTerminateOnDefaultAppDestroy();
  delete g_remote_config_desktop_instance;
  g_remote_config_desktop_instance = nullptr;
  delete g_file_manager;
  g_file_manager = nullptr;
  FutureData::Destroy();
}

#ifndef SWIG
void SetDefaults(const ConfigKeyValueVariant* defaults,
                 size_t number_of_defaults) {
  SetDefaults(defaults, number_of_defaults, nullptr);
}

void SetDefaults(const ConfigKeyValueVariant* defaults,
                 size_t number_of_defaults, const char* config_namespace) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  g_remote_config_desktop_instance->SetDefaults(defaults, number_of_defaults,
                                                config_namespace);
}
#endif  // SWIG

void SetDefaults(const ConfigKeyValue* defaults, size_t number_of_defaults) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  SetDefaults(defaults, number_of_defaults, nullptr);
}

void SetDefaults(const ConfigKeyValue* defaults, size_t number_of_defaults,
                 const char* config_namespace) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  g_remote_config_desktop_instance->SetDefaults(defaults, number_of_defaults,
                                                config_namespace);
}

std::string GetConfigSetting(ConfigSetting setting) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  return g_remote_config_desktop_instance->GetConfigSetting(setting);
}

void SetConfigSetting(ConfigSetting setting, const char* value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  g_remote_config_desktop_instance->SetConfigSetting(setting, value);
}

bool GetBoolean(const char* key) {
  return GetBoolean(key, static_cast<char*>(nullptr),
                    static_cast<ValueInfo*>(nullptr));
}

bool GetBoolean(const char* key, const char* config_namespace) {
  return GetBoolean(key, config_namespace, static_cast<ValueInfo*>(nullptr));
}

bool GetBoolean(const char* key, ValueInfo* info) {
  return GetBoolean(key, static_cast<char*>(nullptr), info);
}

bool GetBoolean(const char* key, const char* config_namespace,
                ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  return g_remote_config_desktop_instance->GetBoolean(key, config_namespace,
                                                      info);
}

int64_t GetLong(const char* key) {
  return GetLong(key, static_cast<char*>(nullptr),
                 static_cast<ValueInfo*>(nullptr));
}

int64_t GetLong(const char* key, const char* config_namespace) {
  return GetLong(key, config_namespace, static_cast<ValueInfo*>(nullptr));
}

int64_t GetLong(const char* key, ValueInfo* info) {
  return GetLong(key, static_cast<char*>(nullptr), info);
}

int64_t GetLong(const char* key, const char* config_namespace,
                ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(0, internal::IsInitialized());
  return g_remote_config_desktop_instance->GetLong(key, config_namespace, info);
}

double GetDouble(const char* key) {
  return GetDouble(key, static_cast<char*>(nullptr),
                   static_cast<ValueInfo*>(nullptr));
}

double GetDouble(const char* key, const char* config_namespace) {
  return GetDouble(key, config_namespace, static_cast<ValueInfo*>(nullptr));
}

double GetDouble(const char* key, ValueInfo* info) {
  return GetDouble(key, static_cast<char*>(nullptr), info);
}

double GetDouble(const char* key, const char* config_namespace,
                 ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(0.0, internal::IsInitialized());
  return g_remote_config_desktop_instance->GetDouble(key, config_namespace,
                                                     info);
}

std::string GetString(const char* key) {
  return GetString(key, static_cast<char*>(nullptr),
                   static_cast<ValueInfo*>(nullptr));
}

std::string GetString(const char* key, const char* config_namespace) {
  return GetString(key, config_namespace, static_cast<ValueInfo*>(nullptr));
}

std::string GetString(const char* key, ValueInfo* info) {
  return GetString(key, static_cast<char*>(nullptr), info);
}

std::string GetString(const char* key, const char* config_namespace,
                      ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  return g_remote_config_desktop_instance->GetString(key, config_namespace,
                                                     info);
}

std::vector<unsigned char> GetData(const char* key) {
  return GetData(key, static_cast<char*>(nullptr),
                 static_cast<ValueInfo*>(nullptr));
}

std::vector<unsigned char> GetData(const char* key,
                                   const char* config_namespace) {
  return GetData(key, config_namespace, static_cast<ValueInfo*>(nullptr));
}

std::vector<unsigned char> GetData(const char* key, ValueInfo* info) {
  return GetData(key, static_cast<char*>(nullptr), info);
}

std::vector<unsigned char> GetData(const char* key,
                                   const char* config_namespace,
                                   ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(std::vector<unsigned char>(),
                         internal::IsInitialized());
  return g_remote_config_desktop_instance->GetData(key, config_namespace, info);
}

std::vector<std::string> GetKeysByPrefix(const char* prefix) {
  return GetKeysByPrefix(prefix, static_cast<char*>(nullptr));
}

std::vector<std::string> GetKeysByPrefix(const char* prefix,
                                         const char* config_namespace) {
  FIREBASE_ASSERT_RETURN(std::vector<std::string>(), internal::IsInitialized());
  return g_remote_config_desktop_instance->GetKeysByPrefix(prefix,
                                                           config_namespace);
}

std::vector<std::string> GetKeys() {
  return GetKeys(static_cast<char*>(nullptr));
}

std::vector<std::string> GetKeys(const char* config_namespace) {
  FIREBASE_ASSERT_RETURN(std::vector<std::string>(), internal::IsInitialized());
  return g_remote_config_desktop_instance->GetKeys(config_namespace);
}

bool ActivateFetched() {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  return g_remote_config_desktop_instance->ActivateFetched();
}

const ConfigInfo& GetInfo() {
  static ConfigInfo config_info;
  FIREBASE_ASSERT_RETURN(config_info, internal::IsInitialized());
  return g_remote_config_desktop_instance->GetInfo();
}

Future<void> Fetch() { return Fetch(kDefaultCacheExpiration); }

Future<void> Fetch(uint64_t cache_expiration_in_seconds) {
  FIREBASE_ASSERT_RETURN(FetchLastResult(), internal::IsInitialized());
  return g_remote_config_desktop_instance->Fetch(cache_expiration_in_seconds);
}

Future<void> FetchLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  return g_remote_config_desktop_instance->FetchLastResult();
}

}  // namespace remote_config
}  // namespace firebase
