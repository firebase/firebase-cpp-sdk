// Copyright 2019 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "firebase_test_framework.h"  // NOLINT

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace firebase_test_framework {

using app_framework::LogDebug;
using app_framework::LogError;
using app_framework::ProcessEvents;

// Default HTTP timeout of 1 minute.
const int kHttpTimeoutSeconds = 60;

// A simple helper function for performing synchronous HTTP/HTTPS requests, used for testing
// purposes only.
static bool SendHttpRequest(const char* method, const char* url,
                            const std::map<std::string, std::string>& headers,
                            const std::string& post_body, int* response_code,
                            std::string* response_str) {
  NSMutableURLRequest* url_request = [[NSMutableURLRequest alloc] init];
  url_request.URL = [NSURL URLWithString:@(url)];
  url_request.HTTPMethod = @(method);
  url_request.timeoutInterval = kHttpTimeoutSeconds;
  if (strcmp(method, "POST") == 0) {
    url_request.HTTPBody = [NSData dataWithBytes:post_body.c_str() length:post_body.length()];
  }
  // Set all the headers.
  for (auto i = headers.begin(); i != headers.end(); ++i) {
    [url_request addValue:@(i->second.c_str()) forHTTPHeaderField:@(i->first.c_str())];
  }
  __block dispatch_semaphore_t sem = dispatch_semaphore_create(0);
  __block NSError* response_error;
  __block NSHTTPURLResponse* http_response;
  __block NSData* response_data;
  LogDebug("Sending HTTP %s request to %s", method, url);
  @try {
    [[NSURLSession.sharedSession
        dataTaskWithRequest:url_request
          completionHandler:^(NSData* __nullable data, NSURLResponse* __nullable response,
                              NSError* __nullable error) {
            response_data = data;
            http_response = (NSHTTPURLResponse*)(response);
            response_error = error;
            dispatch_semaphore_signal(sem);
          }] resume];
  } @catch (NSException* e) {
    LogError("NSURLSession exception: %s", e.reason.UTF8String);
    return false;
  }
  dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

  LogDebug("HTTP status code %ld", http_response.statusCode);
  if (http_response && response_code) {
    *response_code = static_cast<int>(http_response.statusCode);
  }
  std::string response_text =
      response_data.bytes
          ? std::string(reinterpret_cast<const char*>(response_data.bytes), response_data.length)
          : std::string();
  LogDebug("Got response: %s", response_text.c_str());
  if (response_str) {
    *response_str = response_text;
  }
  if (response_error) {
    LogError("HTTP error: %s", response_error.localizedDescription.UTF8String);
    return false;
  }
  return true;
}

// Blocking HTTP request helper function, for testing only.
bool FirebaseTest::SendHttpGetRequest(const char* url,
                                      const std::map<std::string, std::string>& headers,
                                      int* response_code, std::string* response_str) {
  return SendHttpRequest("GET", url, headers, "", response_code, response_str);
}

bool FirebaseTest::SendHttpPostRequest(const char* url,
                                       const std::map<std::string, std::string>& headers,
                                       const std::string& post_body, int* response_code,
                                       std::string* response_str) {
  return SendHttpRequest("POST", url, headers, post_body, response_code, response_str);
}

bool FirebaseTest::OpenUrlInBrowser(const char* url) {
#if TARGET_OS_IOS
  if (strncmp(url, "data:", strlen("data:")) == 0) {
    // Workaround because Safari can't load data: URLs by default.
    // Instead, copy the URL to the clipboard and ask the user to paste it into Safari.

    // data: URLs are in the format data:text/html,<some html data goes here>
    // Preserve everything until the first comma, then URL-encode the rest.
    const char* payload = strchr(url, ',');
    if (payload == nullptr) {
      return false;
    }
    payload++;  // Move past the comma.
    std::string scheme_and_encoding(url);
    scheme_and_encoding.resize(payload - url);
    UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
    pasteboard.string = [@(scheme_and_encoding.c_str())
        stringByAppendingString:[@(payload) stringByAddingPercentEncodingWithAllowedCharacters:
                                                [NSCharacterSet URLHostAllowedCharacterSet]]];
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    UIAlertController* alert =
        [UIAlertController alertControllerWithTitle:@"Paste URL"
                                            message:@"Opening Safari. Please tap twice on the "
                                                    @"address bar and select \"Paste and Go\"."
                                     preferredStyle:UIAlertControllerStyleAlert];
    UIAlertAction* ok = [UIAlertAction actionWithTitle:@"OK"
                                                 style:UIAlertActionStyleDefault
                                               handler:^(UIAlertAction* action) {
                                                 dispatch_semaphore_signal(sem);
                                               }];
    [alert addAction:ok];
    dispatch_async(dispatch_get_main_queue(), ^{
      [[UIApplication sharedApplication].keyWindow.rootViewController presentViewController:alert
                                                                                   animated:YES
                                                                                 completion:nil];
    });
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    return OpenUrlInBrowser("http://");
  } else {
    // Not a data: URL, load it normally.
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block BOOL succeeded = NO;
    NSURL* nsurl = [NSURL URLWithString:@(url)];
    [UIApplication.sharedApplication openURL:nsurl
                                     options:[NSDictionary dictionary]
                           completionHandler:^(BOOL success) {
                             succeeded = success;
                             dispatch_semaphore_signal(sem);
                           }];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    return succeeded ? true : false;
  }
#else // Non-iOS Apple platforms (eg:tvOS)
  return false;
#endif //TARGET_OS_IOS
}

bool FirebaseTest::SetPersistentString(const char* key, const char* value) {
  if (!key) {
    LogError("SetPersistentString: null key is not allowed.");
    return false;
  }
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if (!defaults) {
    return false;
  }
  if (value) {
    [defaults setObject:@(value) forKey:@(key)];
  } else {
    // If value is null, remove this key.
    [defaults removeObjectForKey:@(key)];
  }
  [defaults synchronize];
  return true;
}

bool FirebaseTest::GetPersistentString(const char* key, std::string* value_out) {
  if (!key) {
    return false;
  }
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if (!defaults) {
    return false;
  }
  if (![defaults objectForKey:@(key)]) {
    return false;  // for missing key
  }
  NSString* str = [defaults stringForKey:@(key)];
  if (value_out) {
    *value_out = std::string(str.UTF8String);
  }
  return true;
}

bool FirebaseTest::IsRunningOnEmulator() {
  // On iOS/tvOS it's an easy compile-time definition.
#if TARGET_OS_SIMULATOR
  return true;
#else
  return false;
#endif
}

int FirebaseTest::GetGooglePlayServicesVersion() {
  // No Google Play services on iOS.
  return 0;
}

}  // namespace firebase_test_framework
