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

#import <Foundation/Foundation.h>

@interface FIRAnalytics : NSObject

+ (void)logEventWithName:(nonnull NSString *)name
              parameters:(nullable NSDictionary<NSString *, id> *)parameters;

+ (void)setUserPropertyString:(nullable NSString *)value forName:(nonnull NSString *)name;

+ (void)setUserID:(nullable NSString *)userID;

+ (void)setScreenName:(nullable NSString *)screenName
          screenClass:(nullable NSString *)screenClassOverride;

+ (void)setAnalyticsCollectionEnabled:(BOOL)analyticsCollectionEnabled;

+ (void)setSessionTimeoutInterval:(NSTimeInterval)sessionTimeoutInterval;

+ (nullable NSString *)appInstanceID;

+ (void)resetAnalyticsData;

@end
