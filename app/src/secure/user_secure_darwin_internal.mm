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

#include "app/src/secure/user_secure_darwin_internal.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "app/src/assert.h"
#include "app/src/base64.h"

namespace firebase::app::secure {

namespace {

// Prefix and suffix to add to keychain service name.
constexpr char kServicePrefix[] = "";
constexpr char kServiceSeparator[] = ".";
constexpr char kServiceSuffix1[] = ".firebase.";
constexpr char kServiceSuffix2[] = "";
// For example:
// com.my_company.my_app.firebase_project_id.process_name_hash.firebase.auth
// com.my_company.my_app.firebase_project_id.process_hash.firebase.iid

// Prefix and suffix for the key for NSUserDefaults. domain and service are
// inserted in the middle.
constexpr char kUserDefaultsPrefix[] = "com.google.firebase.";
constexpr char kUserDefaultsSeparator[] = ".";
constexpr char kUserDefaultsSuffix[] = ".has_secure_data";
// For example:
// com.google.firebase.com.my_company.my_app.firebase_project_id.process_hash.auth.has_secure_data

std::string GetProcessId() {
  const char* proc_utf8 = [[NSProcessInfo processInfo] processName].UTF8String;
  std::string process_name = proc_utf8 ? proc_utf8 : "";
  // Deterministic 64-bit FNV-1a hash across process executions.
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char c : process_name) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  uint8_t hash_bytes[sizeof(hash)];
  for (size_t i = 0; i < sizeof(hash); ++i) {
    hash_bytes[i] = static_cast<uint8_t>((hash >> (i * 8)) & 0xFF);
  }
  std::string process_name_hash_binary(
      reinterpret_cast<const char*>(hash_bytes), sizeof(hash_bytes));
  std::string output;
  bool hash_encode_success =
      internal::Base64Encode(process_name_hash_binary, &output);
  FIREBASE_ASSERT(hash_encode_success);
  return output;
}

std::string BuildServiceName(const char* domain, const char* service,
                             const std::string& process_id) {
  const char* safe_domain = domain ? domain : "";
  const char* safe_service = service ? service : "";
  return std::string(kServicePrefix) + safe_service + kServiceSeparator +
         process_id + kServiceSuffix1 + safe_domain + kServiceSuffix2;
}

std::string BuildUserDefaultsKey(const char* domain, const char* service,
                                 const std::string& process_id) {
  const char* safe_domain = domain ? domain : "";
  const char* safe_service = service ? service : "";
  return std::string(kUserDefaultsPrefix) + safe_service +
         kUserDefaultsSeparator + process_id + kUserDefaultsSeparator +
         safe_domain + kUserDefaultsSuffix;
}

NSMutableDictionary* GetQueryForApp(const char* service, const char* app) {
  // Create a query dictionary representing our service and the (optional) app
  // name. If either string fails UTF-8 conversion to NSString, return nil so
  // callers abort rather than executing an unscoped wildcard Keychain query.
  if (!service) {
    return nil;
  }
  NSString* service_str = @(service);
  if (service_str == nil) {
    return nil;
  }
  NSMutableDictionary* query = [[NSMutableDictionary alloc] init];
  query[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
  query[(__bridge id)kSecAttrService] = service_str;
  if (app) {
    NSString* app_str = @(app);
    if (app_str == nil) {
      return nil;
    }
    query[(__bridge id)kSecAttrAccount] = app_str;
  }
  return query;
}

}  // namespace

UserSecureDarwinInternal::UserSecureDarwinInternal(const char* domain,
                                                   const char* service)
    : domain_(domain ? domain : ""),
      service_(BuildServiceName(domain, service, GetProcessId())),
      user_defaults_key_(BuildUserDefaultsKey(domain, service, GetProcessId())) {
}

UserSecureDarwinInternal::~UserSecureDarwinInternal() = default;

bool UserSecureDarwinInternal::UserHasSecureData() const {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* key = @(user_defaults_key_.c_str());
  if (!defaults || !key) {
    // If we can't get NSUserDefaults or convert the key, err on the safe side
    // and return true so we check the keychain directly.
    return true;
  }
  return [defaults boolForKey:key] ? true : false;
}

void UserSecureDarwinInternal::SetUserHasSecureData(bool has_data) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* key = @(user_defaults_key_.c_str());
  if (defaults && key) {
    [defaults setBool:(has_data ? YES : NO) forKey:key];
    [defaults synchronize];
  }
}

std::string UserSecureDarwinInternal::GetKeystoreLocation(
    const std::string& app) const {
  return service_ + "/" + app;
}

std::string UserSecureDarwinInternal::LoadUserData(
    const std::string& app_name) {
  if (!UserHasSecureData()) {
    LogDebug("LoadUserData: User has no data stored.");
    return "";
  }
  std::string keystore_location = GetKeystoreLocation(app_name);
  NSMutableDictionary* query =
      GetQueryForApp(service_.c_str(), app_name.c_str());
  if (query == nil) {
    LogError("LoadUserData: Invalid UTF-8 in service or app_name for %s",
             keystore_location.c_str());
    return "";
  }
  // We want to return the data and attributes.
  query[(__bridge id)kSecReturnData] = @YES;
  query[(__bridge id)kSecReturnAttributes] = @YES;
  // Request up to 2 matches, so we can find out if there is more than one,
  // which is an error.
  query[(__bridge id)kSecMatchLimit] = @2;

  CFTypeRef result = nullptr;
  OSStatus status =
      SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

  if (status == errSecItemNotFound) {
    LogDebug("LoadUserData: Key %s not found", keystore_location.c_str());
    return "";
  } else if (status != noErr) {
    // Some other error occurred.
    NSString* error_string =
        (__bridge_transfer NSString*)SecCopyErrorMessageString(status, NULL);
    LogError("LoadUserData: Error %d reading %s: %s", static_cast<int>(status),
             keystore_location.c_str(),
             (error_string && error_string.UTF8String) ? error_string.UTF8String
                                                       : "Unknown error");
    return "";
  }
  id raw_items = (__bridge_transfer id)result;
  if (raw_items == nil || ![raw_items isKindOfClass:[NSArray class]]) {
    return "";
  }
  NSArray* items = static_cast<NSArray*>(raw_items);
  if (items.count == 0) {
    return "";
  }
  if (items.count > 1) {
    LogError(
        "LoadUserData: Error reading %s: Returned %lu entries instead of 1. "
        "Deleting extraneous entries.",
        keystore_location.c_str(), static_cast<unsigned long>(items.count));
    // Delete all the invalid data so it doesn't mess us up next time.
    DeleteUserData(app_name);
    return "";
  }
  NSDictionary* item = items[0];
  if (![item isKindOfClass:[NSDictionary class]]) {
    return "";
  }
  // Grab the data from the single item in the array.
  NSData* data = item[(__bridge id)kSecValueData];
  if (data == nil || ![data isKindOfClass:[NSData class]] ||
      data.length == 0 || data.bytes == nullptr) {
    return "";
  }
  return std::string(reinterpret_cast<const char*>(data.bytes), data.length);
}

void UserSecureDarwinInternal::SaveUserData(const std::string& app_name,
                                            const std::string& user_data) {
  std::string keystore_location = GetKeystoreLocation(app_name);
  NSMutableDictionary* query =
      GetQueryForApp(service_.c_str(), app_name.c_str());
  if (query == nil) {
    LogError("SaveUserData: Invalid UTF-8 in service or app_name for %s",
             keystore_location.c_str());
    return;
  }
  DeleteUserData(app_name);
  std::string comment = std::string("Firebase ") + domain_ +
                        " persistent user data for " + keystore_location;
  NSString* comment_str = @(comment.c_str());
  if (comment_str == nil) {
    comment_str = @"";
  }
  // Set the binary data, what type of accesibility it should have, and a
  // comment.
  NSDictionary* attributes = @{
    (__bridge id)kSecValueData :
        [NSData dataWithBytes:reinterpret_cast<const void*>(user_data.c_str())
                       length:user_data.length()],
    (__bridge id)kSecAttrAccessible :
        (__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
    (__bridge id)kSecAttrComment : comment_str,
  };

  NSMutableDictionary* combined = [attributes mutableCopy];
  [combined addEntriesFromDictionary:query];

  OSStatus status = SecItemAdd((__bridge CFDictionaryRef)combined, nil);

  if (status != noErr) {
    NSString* error_string =
        (__bridge_transfer NSString*)SecCopyErrorMessageString(status, NULL);
    LogError("SaveUserData: Error %d adding %s: %s", static_cast<int>(status),
             keystore_location.c_str(),
             (error_string && error_string.UTF8String) ? error_string.UTF8String
                                                       : "Unknown error");
    return;
  }

  SetUserHasSecureData(true);
}

void UserSecureDarwinInternal::DeleteData(const char* app_name,
                                          const char* func_name) {
  if (!UserHasSecureData()) {
    LogDebug("%s: User has no data stored.", func_name);
    return;
  }
  std::string keystore_location =
      app_name ? GetKeystoreLocation(app_name) : service_;
  NSMutableDictionary* query = GetQueryForApp(service_.c_str(), app_name);
  if (query == nil) {
    LogError("%s: Invalid UTF-8 in service or app_name for %s", func_name,
             keystore_location.c_str());
    return;
  }

  // In case keychain data is invalid and there is more than one
  // matching entry, set the match limit to delete them all.
  query[(__bridge id)kSecMatchLimit] = [NSNumber numberWithInt:INT_MAX];

  // Delete a specific matching entry.
  OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);

  if (status == errSecItemNotFound) {
    LogDebug("%s: Key %s not found", func_name, keystore_location.c_str());
    return;
  } else if (status != noErr) {
    NSString* error_string =
        (__bridge_transfer NSString*)SecCopyErrorMessageString(status, NULL);
    LogError("%s: Error %d deleting %s: %s", func_name,
             static_cast<int>(status), keystore_location.c_str(),
             (error_string && error_string.UTF8String) ? error_string.UTF8String
                                                       : "Unknown error");
    return;
  }
}

void UserSecureDarwinInternal::DeleteUserData(const std::string& app_name) {
  DeleteData(app_name.c_str(), "DeleteUserData");
}

void UserSecureDarwinInternal::DeleteAllData() {
  DeleteData(nullptr, "DeleteAllData");
  SetUserHasSecureData(false);
}

}  // namespace firebase::app::secure
