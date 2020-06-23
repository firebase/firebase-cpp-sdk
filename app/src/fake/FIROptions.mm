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

#import "app/src/fake/FIROptions.h"

@implementation FIROptions

+ (FIROptions *)defaultOptions {
  FIROptions* options =
      [[FIROptions alloc] initWithGoogleAppID:@"fake google app id from resource"
                                  GCMSenderID:@"fake messaging sender id from resource"];
  options.APIKey = @"fake api key from resource";
  options.bundleID = @"fake bundle ID from resource";
  options.clientID = @"fake client id from resource";
  options.trackingID = @"fake ga tracking id from resource";
  options.projectID = @"fake project id from resource";
  options.androidClientID = @"fake android client id from resource";
  options.googleAppID = @"fake app id from resource";
  options.databaseURL = @"fake database url from resource";
  options.deepLinkURLScheme = @"fake deep link url scheme from resource";
  options.storageBucket = @"fake storage bucket from resource";
  return options;
}

- (instancetype)initWithGoogleAppID:(NSString *)googleAppID
                        GCMSenderID:(NSString *)GCMSenderID {
  self = [super init];
  if (self) {
    _googleAppID = googleAppID;
    _GCMSenderID = GCMSenderID;
  }
  return self;
}

- (id)copyWithZone:(NSZone *)zone {
  FIROptions *newOptions = [[[self class] allocWithZone:zone] init];
  if (newOptions) {
    newOptions.googleAppID = self.googleAppID;
    newOptions.GCMSenderID = self.GCMSenderID;
    newOptions.APIKey = self.APIKey;
    newOptions.bundleID = self.bundleID;
    newOptions.clientID = self.clientID;
    newOptions.trackingID = self.trackingID;
    newOptions.projectID = self.projectID;
    newOptions.androidClientID = self.androidClientID;
    newOptions.googleAppID = self.googleAppID;
    newOptions.databaseURL = self.databaseURL;
    newOptions.deepLinkURLScheme = self.deepLinkURLScheme;
    newOptions.storageBucket = self.storageBucket;
    newOptions.appGroupID = self.appGroupID;
  }
  return newOptions;
}

@end
