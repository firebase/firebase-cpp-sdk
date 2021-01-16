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

#import "messaging/src/ios/fake/FIRMessaging.h"

#include "testing/reporter_impl.h"

NS_ASSUME_NONNULL_BEGIN

@implementation FIRMessagingMessageInfo

- (instancetype)initWithStatus:(FIRMessagingMessageStatus)status {
  self = [super init];
  if (self) {
    _status = status;
  }
  return self;
}

@end

@implementation FIRMessaging

- (instancetype)initInternal {
  self = [super init];
  return self;
}

+ (instancetype)messaging {
  static FIRMessaging *messaging;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    // Start Messaging (Fully initialize in one place).
    messaging = [[FIRMessaging alloc] initInternal];
  });
  return messaging;
}

BOOL is_auto_init_enabled = true;

- (BOOL)isAutoInitEnabled {
  return is_auto_init_enabled;
}

- (void)setAutoInitEnabled:(BOOL)autoInitEnabled {
  is_auto_init_enabled = autoInitEnabled;
}

- (void)retrieveFCMTokenForSenderID:(NSString *)senderID
                         completion:(FIRMessagingFCMTokenFetchCompletion)completion
    NS_SWIFT_NAME(retrieveFCMToken(forSenderID:completion:)) {}

- (void)deleteFCMTokenForSenderID:(NSString *)senderID
                       completion:(FIRMessagingDeleteFCMTokenCompletion)completion
    NS_SWIFT_NAME(deleteFCMToken(forSenderID:completion:)) {}

- (void)connectWithCompletion:(FIRMessagingConnectCompletion)handler
    NS_SWIFT_NAME(connect(handler:))
    __deprecated_msg("Please use the shouldEstablishDirectChannel property instead.") {}

- (void)disconnect
      __deprecated_msg("Please use the shouldEstablishDirectChannel property instead.") {}

+ (NSString *)normalizeTopic:(NSString *)topic {
  return topic;
}

- (void)subscribeToTopic:(NSString *)topic NS_SWIFT_NAME(subscribe(toTopic:)) {
  static const char fake[] = "-[FIRMessaging subscribeToTopic:]";
  std::vector<std::string> args = FakeReporter->GetFakeArgs(fake);
  args.push_back(topic.UTF8String);
  FakeReporter->AddReport(fake, "", args);
}

- (void)subscribeToTopic:(NSString *)topic
              completion:(nullable FIRMessagingTopicOperationCompletion)completion {
  static const char fake[] = "-[FIRMessaging subscribeToTopic:completion:]";
  std::vector<std::string> args = FakeReporter->GetFakeArgs(fake);
  args.push_back(topic.UTF8String);
  FakeReporter->AddReport(fake, "", args);
}

- (void)unsubscribeFromTopic:(NSString *)topic NS_SWIFT_NAME(unsubscribe(fromTopic:)) {
  static const char fake[] = "-[FIRMessaging unsubscribeFromTopic:]";
  std::vector<std::string> args = FakeReporter->GetFakeArgs(fake);
  args.push_back(topic.UTF8String);
  FakeReporter->AddReport(fake, "", args);
}
- (void)unsubscribeFromTopic:(NSString *)topic
                  completion:(nullable FIRMessagingTopicOperationCompletion)completion {
  static const char fake[] = "-[FIRMessaging unsubscribeFromTopic:completion:]";
  std::vector<std::string> args = FakeReporter->GetFakeArgs(fake);
  args.push_back(topic.UTF8String);
  FakeReporter->AddReport(fake, "", args);
}

- (void)sendMessage:(NSDictionary *)message
                 to:(NSString *)receiver
      withMessageID:(NSString *)messageID
         timeToLive:(int64_t)ttl {
  FakeReporter->AddReport("-[FIRMessaging sendMessage:to:withMessageID:timeToLive:]",
      { receiver.UTF8String, messageID.UTF8String,
        [NSString stringWithFormat:@"%lld", ttl].UTF8String });
}

- (FIRMessagingMessageInfo *)appDidReceiveMessage:(NSDictionary *)message {
  FIRMessagingMessageInfo *info =
      [[FIRMessagingMessageInfo alloc] initWithStatus:FIRMessagingMessageStatusUnknown];
  return info;
}

- (void)tokenWithCompletion:(FIRMessagingFCMTokenFetchCompletion)completion
  NS_SWIFT_NAME(retrieveFCMToken(completion:)) {}

- (void)deleteTokenWithCompletion:(FIRMessagingDeleteFCMTokenCompletion)completion
  NS_SWIFT_NAME(deleteFCMToken(completion:)) {}

@end

NS_ASSUME_NONNULL_END
