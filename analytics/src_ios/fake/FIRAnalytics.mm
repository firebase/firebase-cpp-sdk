/*
 * Copyright 2017 Google LLC
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

#import "analytics/src_ios/fake/FIRAnalytics.h"

#include "testing/reporter_impl.h"

@implementation FIRAnalytics

+ (NSString *)stringForValue:(id)value {
  return [NSString stringWithFormat:@"%@", value];
}

+ (NSString *)stringForParameters:(NSDictionary<NSString *, id> *)parameters {
  if ([parameters count] == 0) {
    return @"";
  }

  NSArray<NSString *> *sortedKeys =
      [parameters.allKeys sortedArrayUsingSelector:@selector(compare:)];
  NSMutableString *parameterString = [NSMutableString string];
  for (NSString *key in sortedKeys) {
    [parameterString appendString:key];
    [parameterString appendString:@"="];
    [parameterString appendString:[self stringForValue:parameters[key]]];
    [parameterString appendString:@","];
  }
  // Remove trailing comma from string.
  [parameterString deleteCharactersInRange:NSMakeRange([parameterString length] - 1, 1)];
  return parameterString;
}

+ (void)logEventWithName:(nonnull NSString *)name
              parameters:(nullable NSDictionary<NSString *, id> *)parameters {
  NSString *parameterString = [self stringForParameters:parameters];
  if (parameterString) {
    FakeReporter->AddReport("+[FIRAnalytics logEventWithName:parameters:]",
        { [name UTF8String], [parameterString UTF8String] });
  } else {
    FakeReporter->AddReport("+[FIRAnalytics logEventWithName:parameters:]",
                            { [name UTF8String] });
  }
}

+ (void)setUserPropertyString:(nullable NSString *)value forName:(nonnull NSString *)name {
  FakeReporter->AddReport("+[FIRAnalytics setUserPropertyString:forName:]",
                          { [name UTF8String], value ? [value UTF8String] : "nil" });
}

+ (void)setUserID:(nullable NSString *)userID {
  FakeReporter->AddReport("+[FIRAnalytics setUserID:]", { userID ? [userID UTF8String] : "nil" });
}

+ (void)setScreenName:(nullable NSString *)screenName
          screenClass:(nullable NSString *)screenClassOverride {
  FakeReporter->AddReport("+[FIRAnalytics setScreenName:screenClass:]",
      { screenName ? [screenName UTF8String] : "nil",
        screenClassOverride ? [screenClassOverride UTF8String] : "nil" });
}

+ (void)setSessionTimeoutInterval:(NSTimeInterval)sessionTimeoutInterval {
  FakeReporter->AddReport(
      "+[FIRAnalytics setSessionTimeoutInterval:]",
      {[[NSString stringWithFormat:@"%.03f", sessionTimeoutInterval] UTF8String]});
}

+ (void)setAnalyticsCollectionEnabled:(BOOL)analyticsCollectionEnabled {
  FakeReporter->AddReport("+[FIRAnalytics setAnalyticsCollectionEnabled:]",
                          {analyticsCollectionEnabled ? "YES" : "NO"});
}

+ (NSString *)appInstanceID {
  FakeReporter->AddReport("+[FIRAnalytics appInstanceID]", {});
  return @"FakeAnalyticsInstanceId0";
}

+ (void)resetAnalyticsData {
  FakeReporter->AddReport("+[FIRAnalytics resetAnalyticsData]", {});
}

@end
