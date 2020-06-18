/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import <UIKit/UIApplication.h>
#import <XCTest/XCTest.h>
#include "app/src/util_ios.h"

@interface SwizzlingTests : XCTestCase
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property(strong, nonatomic) NSMutableArray *selectorList;
@end

@implementation AppDelegate

- (instancetype)init {
  if (self = [super init]) {
    _selectorList = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
  // Save the selectors and arguments that were called this way, to validate against later.
  const char *selName = sel_getName([invocation selector]);
  NSMutableString *toAdd = [NSMutableString stringWithUTF8String:selName];
  int numArgs = [[invocation methodSignature] numberOfArguments];
  for (int i = 2; i < numArgs; i++) {
    __unsafe_unretained id arg;
    [invocation getArgument:&arg atIndex:i];
    [toAdd appendString:[NSString stringWithFormat:@"|%p", arg]];
  }
  [_selectorList addObject:toAdd];
}

@end

@implementation SwizzlingTests

- (void)testForwardInvocationPassThrough {
  AppDelegate *appDelegate = [[AppDelegate alloc] init];

  UIApplication *application = [UIApplication sharedApplication];
  NSURL *url = [[NSURL alloc] init];
  NSUserActivity *activity = [[NSUserActivity alloc] initWithActivityType:@"myactivity"];
  void (^handler)(NSArray *);
  NSData *data = [[NSData alloc] init];
  NSError *error = [[NSError alloc] init];
  firebase::util::UIBackgroundFetchResultFunction fetchHandler;
  NSDictionary *dict = [[NSDictionary alloc] init];
  NSString *string = @"TestString";
  id testId = data;

  NSMutableArray *expectedList = [[NSMutableArray alloc] init];

  // From invites_ios_startup.mm
  [expectedList addObject:[NSString stringWithFormat:@"application:openURL:options:|%p|%p|%p",
                                                     application, url, dict]];
  [appDelegate application:application openURL:url options:dict];

  [expectedList
      addObject:[NSString stringWithFormat:
                              @"application:openURL:sourceApplication:annotation:|%p|%p|%p|%p",
                              application, url, string, testId]];
  [appDelegate application:application openURL:url sourceApplication:string annotation:testId];

  [expectedList
      addObject:[NSString stringWithFormat:
                              @"application:continueUserActivity:restorationHandler:|%p|%p|%p",
                              application, activity, handler]];
  [appDelegate application:application continueUserActivity:activity restorationHandler:handler];

  [expectedList
      addObject:[NSString stringWithFormat:@"applicationDidBecomeActive:|%p", application]];
  [appDelegate applicationDidBecomeActive:application];

  // From instance_id.mm
  [expectedList
      addObject:[NSString
                    stringWithFormat:
                        @"application:didRegisterForRemoteNotificationsWithDeviceToken:|%p|%p",
                        application, data]];
  [appDelegate application:application didRegisterForRemoteNotificationsWithDeviceToken:data];

  // From messaging.mm
  [expectedList
      addObject:[NSString stringWithFormat:@"application:didFinishLaunchingWithOptions:|%p|%p",
                                           application, dict]];
  [appDelegate application:application didFinishLaunchingWithOptions:dict];

  [expectedList
      addObject:[NSString stringWithFormat:@"applicationDidEnterBackground:|%p", application]];
  [appDelegate applicationDidEnterBackground:application];

  [expectedList
      addObject:[NSString
                    stringWithFormat:
                        @"application:didFailToRegisterForRemoteNotificationsWithError:|%p|%p",
                        application, error]];
  [appDelegate application:application didFailToRegisterForRemoteNotificationsWithError:error];

  [expectedList
      addObject:[NSString stringWithFormat:@"application:didReceiveRemoteNotification:|%p|%p",
                                           application, dict]];
  [appDelegate application:application didReceiveRemoteNotification:dict];

  [expectedList addObject:[NSString stringWithFormat:@"application:didReceiveRemoteNotification:"
                                                     @"fetchCompletionHandler:|%p|%p|%p",
                                                     application, dict, fetchHandler]];
  [appDelegate application:application
      didReceiveRemoteNotification:dict
            fetchCompletionHandler:fetchHandler];

  XCTAssertEqualObjects([appDelegate selectorList], expectedList);
}

@end
