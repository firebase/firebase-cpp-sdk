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

#include "testlab/src/include/firebase/testlab.h"

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/reference_count.h"
#include "app/src/util_ios.h"
#include "testlab/src/common/common.h"
#include "testlab/src/ios/custom_results.h"

using firebase::internal::ReferenceCount;
using firebase::internal::ReferenceCountLock;

namespace firebase {
namespace test_lab {
namespace game_loop {

static ReferenceCount g_initializer;  // NOLINT

// String constants for iOS game loops
static const char* kFtlScheme = "firebase-game-loop";
static const char* kFtlCompleteUrl = "firebase-game-loop-complete://";
static const char* kScenario = "scenario";

namespace internal {

// Determine whether the test lab module is initialized.
bool IsInitialized() { return g_initializer.references() > 0; }

}  // namespace internal

static ::firebase::util::ClassMethodImplementationCache& SwizzledMethodCache() {
  static ::firebase::util::ClassMethodImplementationCache* g_swizzled_method_cache;
  return *::firebase::util::ClassMethodImplementationCache::GetCreateCache(
      &g_swizzled_method_cache);
}

// Initialize the game loop API
void Initialize(const ::firebase::App& app) {
  ReferenceCountLock<ReferenceCount> ref_count(&g_initializer);
  if (ref_count.references() != 0) {
    LogWarning("Test Lab API already initialized");
    return;
  }
  LogDebug("Firebase Test Lab API initializing");
  internal::CreateOrOpenLogFile();
  ref_count.AddReference();
}

// Release resources associated with the game loop API
void Terminate() {
  ReferenceCountLock<ReferenceCount> ref_count(&g_initializer);
  if (ref_count.references() == 0) {
    LogWarning("Test Lab API was never initialized");
    return;
  }
  LogDebug("Terminating the Firebase Test Lab API");
  if (ref_count.references() == 1) {
    internal::CloseLogFile();
    internal::TerminateCommon();
  }
  ref_count.RemoveReference();
}

// Return the current scenario number of the game loop test
int GetScenario() {
  if (!internal::IsInitialized()) return 0;
  return internal::GetScenario();
}

// Log formatted text to the game loop's custom results
void LogText(const char* format, ...) {
  ReferenceCountLock<ReferenceCount> ref_count(&g_initializer);
  if (GetScenario() == 0) return;
  va_list args;
  va_start(args, format);
  internal::LogText(format, args);
  va_end(args);
}

// End a game loop scenario with an outcome
void FinishScenario(::firebase::test_lab::game_loop::ScenarioOutcome outcome) {
  if (GetScenario() == 0) return;
  FILE* custom_results_file = internal::CreateCustomResultsFile(GetScenario());
  if (custom_results_file == nullptr) {
    LogError("Could not obtain the custom results file");
  } else {
    internal::OutputResult(outcome, custom_results_file);
  }
  Terminate();
  UIApplication* app = [UIApplication sharedApplication];
  [app openURL:[NSURL URLWithString:@(kFtlCompleteUrl)]
                options:@{}
      completionHandler:^(BOOL success){
          // TODO(brandonmorris): Investigate a graceful way to exit the application, or make a
          // special exception within FTL to signify finishing a scenario is not a crash.
      }];
}

// Retrieve the scenario from the URL if present
static int ParseScenarioFromUrl(NSURL* url) {
  if ([url.scheme isEqualToString:(@(kFtlScheme))]) {
    NSURLComponents* components = [NSURLComponents componentsWithURL:url
                                             resolvingAgainstBaseURL:YES];
    for (NSURLQueryItem* item in [components queryItems]) {
      if ([item.name isEqualToString:@(kScenario)]) {
        int scenario = (int)[item.value integerValue];
        NSLog(@"%@", [NSString stringWithFormat:@"Found scenario %d", scenario]);
        return scenario > 0 ? scenario : 0;
      }
    }
  }
  LogWarning("No game loop scenario could be parsed from the URL scheme");
  return 0;
}

// Implementation of application:openURL:options that replaces the original
// app delegate's implementation.
static BOOL AppDelegateApplicationOpenURLOptions(id self, SEL selectorValue,
                                                 UIApplication* application, NSURL* url,
                                                 NSDictionary* options) {
  LogDebug("Parsing URL for application:openURL:options:");
  internal::SetScenario(ParseScenarioFromUrl(url));

  // Some applications / frameworks (like Unity) do not handle nil arguments for url and options
  // so create empty objects to prevent them from failing.
  if (!url) url = [[NSURL alloc] init];
  if (!options) options = @{};

  IMP app_delegate_application_open_url_options =
      SwizzledMethodCache().GetMethodForObject(self, @selector(application:openURL:options:));
  if (app_delegate_application_open_url_options) {
    return (
        (util::AppDelegateApplicationOpenUrlOptionsFunc)app_delegate_application_open_url_options)(
        self, selectorValue, application, url, options);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature* signature = [[self class] instanceMethodSignatureForSelector:selectorValue];
    NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selectorValue];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&url atIndex:3];
    [invocation setArgument:&options atIndex:4];
    [self forwardInvocation:invocation];
    // Read the return value from the invocation.
    BOOL ret;
    [invocation getReturnValue:&ret];
    return ret;
  }
  return YES;
}

static BOOL AppDelegateApplicationOpenUrlSourceApplicationAnnotation(id self, SEL selectorValue,
                                                                     UIApplication* application,
                                                                     NSURL* url,
                                                                     NSString* sourceApplication,
                                                                     id annotation) {
  internal::SetScenario(ParseScenarioFromUrl(url));

  // Some applications / frameworks (like Unity) do not handle nil arguments for url,
  // sourceApplication and annotation, so create empty objects to prevent them from failing.
  if (!url) url = [[NSURL alloc] init];
  if (!sourceApplication) sourceApplication = @"";
  if (!annotation) annotation = [[NSString alloc] init];
  IMP app_delegate_application_open_url_source_application_annotation =
      SwizzledMethodCache().GetMethodForObject(
          self, @selector(application:openURL:sourceApplication:annotation:));
  if (app_delegate_application_open_url_source_application_annotation) {
    return ((util::AppDelegateApplicationOpenUrlSourceApplicationAnnotationFunc)
                app_delegate_application_open_url_source_application_annotation)(
        self, selectorValue, application, url, sourceApplication, annotation);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature* signature = [[self class] instanceMethodSignatureForSelector:selectorValue];
    NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selectorValue];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&url atIndex:3];
    [invocation setArgument:&sourceApplication atIndex:4];
    [invocation setArgument:&annotation atIndex:5];
    [self forwardInvocation:invocation];
    // Read the return value from the invocation.
    BOOL ret;
    [invocation getReturnValue:&ret];
    return ret;
  }
  return YES;
}

// Hook all AppDelegate methods that FTL requires to intercept the game loop
// launch.
//
// The user of the library provides the class which implements AppDelegate protocol so this
// method hooks methods of user's UIApplication class in order to intercept events required for
// FTL. The alternative to this procedure would require the user to implement boilerplate code
// in their UIApplication in order to plumb in FTL.
//
// The following methods are replaced in order to intercept AppDelegate events:
// - (BOOL)application:openURL:sourceApplication:annotation:
// - (BOOL)application:openURL:options:
static void HookAppDelegateMethods(Class clazz) {
  Class method_encoding_class = [FIRSAMAppDelegate class];
  auto& method_cache = SwizzledMethodCache();
  // application:openURL:options: is called in preference to
  // application:openURL:sourceApplication:annotation: so if the UIApplicationDelegate does not
  // implement application:openURL:options:, do not hook it.
  method_cache.ReplaceOrAddMethod(clazz, @selector(application:openURL:options:),
                                  (IMP)AppDelegateApplicationOpenURLOptions, method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(application:openURL:sourceApplication:annotation:),
      (IMP)AppDelegateApplicationOpenUrlSourceApplicationAnnotation, method_encoding_class);
}

}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase

// Category for UIApplication that is used to hook methods in all classes.
// Category +load() methods are called after all class load methods in each Mach-O
// (see call_load_methods() in
// http://www.opensource.apple.com/source/objc4/objc4-274/runtime/objc-runtime.m)
@implementation UIApplication (FIRFTL)
+ (void)load {
  ::firebase::LogDebug("Loading UIApplication FIRFTL category");
  ::firebase::util::ForEachAppDelegateClass(^(Class clazz) {
    ::firebase::test_lab::game_loop::HookAppDelegateMethods(clazz);
  });
}
@end
