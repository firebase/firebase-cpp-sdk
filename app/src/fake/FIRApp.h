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

#import <Foundation/Foundation.h>

extern "C" {

// Test method to create an application using the specified name and default
// options.
void FIRAppCreateUsingDefaultOptions(const char* name);
// Test method to clear all app instances.
void FIRAppResetApps();

}

@class FIROptions;

typedef void (^FIRAppVoidBoolCallback)(BOOL success);

/**
 * A fake Firebase App class for unit-testing.
 */
@interface FIRApp : NSObject

// Test method to clear all FIRApp instances.
+ (void)resetApps;

+ (void)configure;

+ (void)configureWithOptions:(FIROptions *)options;

+ (void)configureWithName:(NSString *)name options:(FIROptions *)options;

+ (FIRApp *)defaultApp;

+ (FIRApp *)appNamed:(NSString *)name;

- (void)deleteApp:(FIRAppVoidBoolCallback)completion;

@property(nonatomic, copy, readonly) NSString *name;

@property(nonatomic, copy, readonly) FIROptions *options;

@property(nonatomic, readwrite, getter=isDataCollectionDefaultEnabled)
    BOOL dataCollectionDefaultEnabled;

@end
