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

#include "messaging/tests/messaging_test_util.h"

#import <UIKit/UIKit.h>

#include "app/src/log.h"
#include "messaging/src/include/firebase/messaging.h"

#import "messaging/src/ios/fake/FIRMessaging.h"

namespace firebase {
namespace messaging {

// Message keys.
static NSString *const kFrom = @"from";
static NSString *const kTo = @"to";
static NSString *const kCollapseKey = @"collapse_key";
static NSString *const kMessageID = @"gcm.message_id";
static NSString *const kMessageType = @"message_type";
static NSString *const kPriority = @"priority";
static NSString *const kTimeToLive = @"time_to_live";
static NSString *const kError = @"error";
static NSString *const kErrorDescription = @"error_description";

// Notification keys.
static NSString *const kTitle = @"title";
static NSString *const kBody = @"body";
static NSString *const kSound = @"sound";
static NSString *const kBadge = @"badge";

// Dual purpose body text or data dictionary.
static NSString *const kAlert = @"alert";


void InitializeMessagingTest() {}

void TerminateMessagingTest() {
  [FIRMessaging messaging].FCMToken = nil;
}

void OnTokenReceived(const char* tokenstr) {
  [FIRMessaging messaging].FCMToken = @(tokenstr);
  [[FIRMessaging messaging].delegate messaging:[FIRMessaging messaging]
                   didReceiveRegistrationToken:@(tokenstr)];
}

void SleepMessagingTest(double seconds) {
  // We want the main loop to process messages while we wait.
  [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:seconds]];
}

void OnMessageReceived(const Message& message) {
  NSMutableDictionary<NSString*, id>* userData = [NSMutableDictionary dictionary];
  userData[kMessageID] = @(message.message_id.c_str());
  userData[kTo] = @(message.to.c_str());
  userData[kFrom] = @(message.from.c_str());
  userData[kCollapseKey] = @(message.collapse_key.c_str());
  userData[kMessageType] = @(message.message_type.c_str());
  userData[kPriority] = @(message.priority.c_str());
  userData[kTimeToLive] = @(message.time_to_live);
  userData[kError] = @(message.error.c_str());
  userData[kErrorDescription] = @(message.error_description.c_str());
  for (const auto& entry : message.data) {
    userData[@(entry.first.c_str())] = @(entry.second.c_str());
  }

  if (message.notification) {
    NSMutableDictionary<NSString*, id>* alert = [NSMutableDictionary dictionary];
    alert[kTitle] = @(message.notification->title.c_str());
    alert[kBody] = @(message.notification->body.c_str());
    NSMutableDictionary* aps = [NSMutableDictionary dictionary];
    aps[kSound] = @(message.notification->sound.c_str());
    aps[kBadge] = @(message.notification->badge.c_str());
    aps[kAlert] = alert;
    userData[@"aps"] = aps;
  }
  [[[UIApplication sharedApplication] delegate] application:[UIApplication sharedApplication]
                               didReceiveRemoteNotification:userData];
}

void OnMessageSent(const char* message_id) {}

void OnMessageSentError(const char* message_id, const char* error) {}

}  // namespace messaging
}  // namespace firebase
