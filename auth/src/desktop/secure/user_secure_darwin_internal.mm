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

#include "auth/src/desktop/secure/user_secure_darwin_internal.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace firebase {
namespace auth {
namespace secure {

static const char kServicePrefix[] = "firebase.auth.cpp.cred/";
static const int kMaxAllowedKeychainEntries = INT_MAX;

UserSecureDarwinInternal::UserSecureDarwinInternal(const char* service) {
  service_ = std::string(kServicePrefix) + service;
}

UserSecureDarwinInternal::~UserSecureDarwinInternal() {}

NSMutableDictionary* GetQueryForApp(const char* service, const char* app) {
  // Create a query dictionary representing our service and the (optional) app name.
  // You can further narrow down this query to use it for saving or loading data.
  NSMutableDictionary* query = [[NSMutableDictionary alloc] init];
  query[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
  query[(__bridge id)kSecAttrService] = @(service);
  if (app) {
    query[(__bridge id)kSecAttrAccount] = @(app);
  }
  return query;
}

std::string UserSecureDarwinInternal::GetKeystoreLocation(const std::string& app) {
  return service_ + "/" + app;
}

std::string UserSecureDarwinInternal::LoadUserData(const std::string& app_name) {
  NSMutableDictionary* query = GetQueryForApp(service_.c_str(), app_name.c_str());
  std::string keystore_location = GetKeystoreLocation(app_name);
  // We want to return the data and attributes.
  query[(__bridge id)kSecReturnData] = @YES;
  query[(__bridge id)kSecReturnAttributes] = @YES;
  // Request up to 2 matches, so we can find out if there is more than one, which is an error.
  query[(__bridge id)kSecMatchLimit] = @2;

  CFArrayRef result = NULL;
  OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef*)&result);

  if (status == errSecItemNotFound) {
    LogDebug("LoadUserData: Key %s not found", keystore_location.c_str());
    return "";
  } else if (status != noErr) {
    // Some other error occurred.
    NSString* error_string = (__bridge_transfer NSString*)SecCopyErrorMessageString(status, NULL);
    LogError("LoadUserData: Error %d reading %s: %s", status, keystore_location.c_str(),
             error_string.UTF8String);
    return "";
  }
  NSArray* items = (__bridge_transfer NSArray*)result;
  if (items.count > 1) {
    LogError("LoadUserData: Error reading %s: Returned %d entries instead of 1. Deleting "
             "extraneous entries.",
             keystore_location.c_str(), items.count);
    // Delete all the invalid data so it doesn't mess us up next time.
    DeleteUserData(app_name);
    return "";
  }
  NSDictionary* item = items[0];
  // Grab the data from the single item in the array.
  NSData* data = item[(__bridge id)kSecValueData];
  return std::string(reinterpret_cast<const char*>(data.bytes), data.length);
}

void UserSecureDarwinInternal::SaveUserData(const std::string& app_name,
                                            const std::string& user_data) {
  DeleteUserData(app_name);
  NSMutableDictionary* query = GetQueryForApp(service_.c_str(), app_name.c_str());
  std::string keystore_location = GetKeystoreLocation(app_name);
  std::string comment =
      std::string("Firebase Auth persistent user data for ") + GetKeystoreLocation(app_name);
  // Set the binary data, what type of accesibility it should have, and a comment.
  NSDictionary* attributes = @{
    (__bridge id)
    kSecValueData : [NSData dataWithBytes:reinterpret_cast<const void*>(user_data.c_str())
                                   length:user_data.length()],
    (__bridge id)kSecAttrAccessible : (__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
    (__bridge id)kSecAttrComment : @(comment.c_str()),
  };

  NSMutableDictionary* combined = [attributes mutableCopy];
  [combined addEntriesFromDictionary:query];

  OSStatus status = SecItemAdd((__bridge CFDictionaryRef)combined, nil);

  if (status != noErr) {
    NSString* error_string = (__bridge_transfer NSString*)SecCopyErrorMessageString(status, NULL);
    LogError("SaveUserData: Error %d adding %s: %s", status, keystore_location.c_str(),
             error_string.UTF8String);
    return;
  }
}

void UserSecureDarwinInternal::DeleteData(const char* app_name, const char* func_name) {
  NSMutableDictionary* query = GetQueryForApp(service_.c_str(), app_name);
  std::string keystore_location = app_name ? GetKeystoreLocation(app_name) : service_;

  // In case keychain data is invalid and there is more than one
  // matching entry, set the match limit to delete them all.
  query[(__bridge id)kSecMatchLimit] = [NSNumber numberWithInt:INT_MAX];

  // Delete a specific matching entry.
  OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);

  if (status == errSecItemNotFound) {
    LogDebug("%s: Key %s not found", keystore_location.c_str());
    return;
  } else if (status != noErr) {
    NSString* error_string = (__bridge_transfer NSString*)SecCopyErrorMessageString(status, NULL);
    LogError("%s: Error %d deleting %s: %s", status, keystore_location.c_str(),
             error_string.UTF8String);
    return;
  }
}

void UserSecureDarwinInternal::DeleteUserData(const std::string& app_name) {
  DeleteData(app_name.c_str(), "DeleteUserData");
}

void UserSecureDarwinInternal::DeleteAllData() { DeleteData(nullptr, "DeleteAllData"); }

}  // namespace secure
}  // namespace auth
}  // namespace firebase
