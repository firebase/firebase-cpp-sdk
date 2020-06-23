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

#import "app/src/fake/FIRApp.h"

#import "app/src/fake/FIROptions.h"

static NSString *kFIRDefaultAppName = @"__FIRAPP_DEFAULT";

@implementation FIRApp

@synthesize options = _options;
BOOL _dataCollectionEnabled;

static NSMutableDictionary *sAllApps;

- (instancetype)initInstanceWithName:(NSString *)name options:(FIROptions *)options {
  self = [super init];
  if (self) {
    _name = [name copy];
    _options = [options copy];
    _dataCollectionEnabled = YES;
  }
  return self;
}

+ (void)resetApps {
  if (sAllApps) [sAllApps removeAllObjects];
}

+ (void)configure {
  return [FIRApp configureWithOptions:[FIROptions defaultOptions]];
}

+ (void)configureWithOptions:(FIROptions *)options {
  return [FIRApp configureWithName:kFIRDefaultAppName options:options];
}

+ (void)configureWithName:(NSString *)name options:(FIROptions *)options {
  FIRApp *app = [[FIRApp alloc] initInstanceWithName:name options:options];
  if (!sAllApps) sAllApps = [[NSMutableDictionary alloc] init];
  sAllApps[app.name] = app;
}

+ (FIRApp *)defaultApp {
  return sAllApps ? sAllApps[kFIRDefaultAppName] : nil;
}

+ (FIRApp *)appNamed:(NSString *)name {
  return sAllApps ? sAllApps[name] : nil;
}

- (void)deleteApp:(FIRAppVoidBoolCallback)completion {
  if (sAllApps) {
    [sAllApps removeObjectForKey:self.name];
  }
  completion(TRUE);
}

- (void)setDataCollectionDefaultEnabled:(BOOL)dataCollectionDefaultEnabled {
  _dataCollectionEnabled = dataCollectionDefaultEnabled;
}

- (BOOL)isDataCollectionDefaultEnabled {
  return _dataCollectionEnabled;
}

static NSMutableDictionary* sRegisteredLibraries = [[NSMutableDictionary alloc] init];

+ (void)registerLibrary:(nonnull NSString *)library withVersion:(nonnull NSString *)version {
  if (sRegisteredLibraries.count == 0) sRegisteredLibraries[@"fire-ios"] = @"1.2.3";
  sRegisteredLibraries[library] = version;
}

+ (NSString *)firebaseUserAgent {
  NSMutableArray<NSString *> *libraries =
      [[NSMutableArray<NSString *> alloc] initWithCapacity:sRegisteredLibraries.count];
  for (NSString *libraryName in sRegisteredLibraries) {
    [libraries
        addObject:[NSString stringWithFormat:@"%@/%@", libraryName,
                            sRegisteredLibraries[libraryName]]];
  }
  [libraries sortUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
  return [libraries componentsJoinedByString:@" "];
}

@end

void FIRAppCreateUsingDefaultOptions(const char* name) {
  [FIRApp configureWithName:@(name) options:[FIROptions defaultOptions]];
}

void FIRAppResetApps() {
  [FIRApp resetApps];
}
