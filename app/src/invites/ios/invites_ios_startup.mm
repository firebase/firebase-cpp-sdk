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

#include "app/src/invites/ios/invites_ios_startup.h"

#include <algorithm>
#include <vector>

#include "app/src/log.h"
#include "app/src/util_ios.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

namespace firebase {
namespace invites {

static ::firebase::util::ClassMethodImplementationCache& SwizzledMethodCache() {
  static ::firebase::util::ClassMethodImplementationCache *g_swizzled_method_cache;
  return *::firebase::util::ClassMethodImplementationCache::GetCreateCache(
       &g_swizzled_method_cache);
}

extern "C" {

static BOOL AppDelegateApplicationOpenUrlSourceApplicationAnnotation(id self, SEL selectorValue,
                                                                     UIApplication *application,
                                                                     NSURL *url,
                                                                     NSString *sourceApplication,
                                                                     id annotation) {
  BOOL ret = internal::InvitesIosStartup::OpenUrl(application, url, sourceApplication, annotation);

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
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selectorValue];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selectorValue];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&url atIndex:3];
    [invocation setArgument:&sourceApplication atIndex:4];
    [invocation setArgument:&annotation atIndex:5];
    [self forwardInvocation:invocation];
    // Read the return value from the invocation.
    [invocation getReturnValue:&ret];
  }
  return ret;
}

static BOOL AppDelegateApplicationContinueUserActivityRestorationHandler(
    id self, SEL selectorValue, UIApplication *application, NSUserActivity *userActivity,
    void (^restorationHandler)(NSArray *)) {
  BOOL ret = internal::InvitesIosStartup::ContinueUserActivity(application, userActivity,
                                                               restorationHandler);

  // Some applications / frameworks may not handle nil arguments for userActivity,
  // so create an empty object to prevent them from failing.
  if (!userActivity) userActivity = [[NSUserActivity alloc] init];
  IMP app_delegate_application_continue_user_activity_restoration_handler =
      SwizzledMethodCache().GetMethodForObject(
          self, @selector(application:continueUserActivity:restorationHandler:));
  if (app_delegate_application_continue_user_activity_restoration_handler) {
    return ((util::AppDelegateApplicationContinueUserActivityRestorationHandlerFunc)
                app_delegate_application_continue_user_activity_restoration_handler)(
        self, selectorValue, application, userActivity, restorationHandler);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selectorValue];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selectorValue];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&userActivity atIndex:3];
    [invocation setArgument:&restorationHandler atIndex:4];
    [self forwardInvocation:invocation];
    // Read the return value from the invocation.
    [invocation getReturnValue:&ret];
  }
  return ret;
}

static BOOL AppDelegateApplicationOpenUrlOptions(id self, SEL selectorValue,
                                                 UIApplication *application, NSURL *url,
                                                 NSDictionary *options) {
  BOOL ret = internal::InvitesIosStartup::OpenUrl(application, url, options);

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
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selectorValue];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selectorValue];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&url atIndex:3];
    [invocation setArgument:&options atIndex:4];
    [self forwardInvocation:invocation];
    // Read the return value from the invocation.
    [invocation getReturnValue:&ret];
  }
  return ret;
}

// Fetch link when entering foreground.
static void AppDelegateApplicationDidBecomeActive(id self, SEL selectorValue,
                                                  UIApplication *application) {
  internal::InvitesIosStartup::DidBecomeActive(application);
  IMP app_delegate_application_did_become_active =
      SwizzledMethodCache().GetMethodForObject(self, @selector(applicationDidBecomeActive:));
  if (app_delegate_application_did_become_active) {
    ((util::AppDelegateApplicationDidBecomeActiveFunc)app_delegate_application_did_become_active)(
        self, selectorValue, application);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selectorValue];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selectorValue];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [self forwardInvocation:invocation];
  }
}

// Hook all AppDelegate methods that Firebase Invites requires to intercept incoming invites and
// dynamic links.
//
// The user of the library provides the class which implements AppDelegate protocol so this method
// hooks methods of user's UIApplication class in order to intercept events required for Invites.
// The alternative to this procedure would require the user to implement boilerplate code in their
// UIApplication in order to plumb in Firebase Invites.
//
// The following methods are replaced in order to intercept AppDelegate events:
// - (BOOL)application:openURL:sourceApplication:annotation:
// - (BOOL)application:openURL:options:
// - (BOOL)application:continueUserActivity:restorationHandler:
static void HookAppDelegateMethods(Class clazz) {
  Class method_encoding_class = [FIRSAMAppDelegate class];
  auto& method_cache = SwizzledMethodCache();
  // application:openURL:options: is called in preference to
  // application:openURL:sourceApplication:annotation: so if the UIApplicationDelegate does not
  // implement application:openURL:options:, do not hook it.
  method_cache.ReplaceMethod(
      clazz, @selector(application:openURL:options:), (IMP)AppDelegateApplicationOpenUrlOptions,
      method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(application:openURL:sourceApplication:annotation:),
      (IMP)AppDelegateApplicationOpenUrlSourceApplicationAnnotation, method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(application:continueUserActivity:restorationHandler:),
      (IMP)AppDelegateApplicationContinueUserActivityRestorationHandler, method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(applicationDidBecomeActive:), (IMP)AppDelegateApplicationDidBecomeActive,
      method_encoding_class);
}

}  // extern "C"

namespace internal {

std::vector<InvitesIosStartup *> *InvitesIosStartup::s_invites_ios_startups = nullptr;

// Register this object with the set of instances that are called from
// static methods (e.g OpenUrl) of this class.
// This method does nothing if the object is already registered.
void InvitesIosStartup::Register() {
  if (s_invites_ios_startups) {
    auto it = std::find(s_invites_ios_startups->begin(),
                        s_invites_ios_startups->end(), this);
    if (it != s_invites_ios_startups->end()) return;
  } else {
    s_invites_ios_startups = new std::vector<InvitesIosStartup *>();
  }
  s_invites_ios_startups->push_back(this);
  std::sort(s_invites_ios_startups->begin(), s_invites_ios_startups->end(),
            [](InvitesIosStartup *lhs, InvitesIosStartup *rhs) {
              return lhs->priority_ < rhs->priority_;
            });
}

// Unregister this object from the set of instances that are called from
// static methods (e.g OpenUrl) of this class.
// This method does nothing if the object is already unregistered.
void InvitesIosStartup::Unregister() {
  if (!s_invites_ios_startups) return;
  auto it = std::find(s_invites_ios_startups->begin(),
                      s_invites_ios_startups->end(), this);
  // If the object isn't in the s_invites_ios_startups vector, do nothing.
  if (it != s_invites_ios_startups->end()) {
    s_invites_ios_startups->erase(it);
  }
  if (s_invites_ios_startups->size() == 0) {
    delete s_invites_ios_startups;
    s_invites_ios_startups = nullptr;
  }
}

// Call HandleDidBecomeActive methods of registered instances of this class.
void InvitesIosStartup::DidBecomeActive(UIApplication *application) {
  if (s_invites_ios_startups) {
    for (auto it = s_invites_ios_startups->begin(); it != s_invites_ios_startups->end(); ++it) {
      (*it)->HandleDidBecomeActive(application);
    }
  }
}

// Call OpenUrl methods of registered instances of this class.
BOOL InvitesIosStartup::OpenUrl(UIApplication *application, NSURL *url,
                                NSString *sourceApplication, id annotation) {
  if (s_invites_ios_startups) {
    for (auto it = s_invites_ios_startups->begin(); it != s_invites_ios_startups->end(); ++it) {
      if ((*it)->HandleOpenUrl(application, url, sourceApplication, annotation)) return YES;
    }
  }
  return NO;
}

BOOL InvitesIosStartup::OpenUrl(UIApplication *application, NSURL *url,
                                NSDictionary *options) {
  if (s_invites_ios_startups) {
    for (auto it = s_invites_ios_startups->begin(); it != s_invites_ios_startups->end(); ++it) {
      if ((*it)->HandleOpenUrl(application, url, options)) return YES;
    }
  }
  return NO;
}

// Call ContinueUserActivity methods of registered instances of this class.
BOOL InvitesIosStartup::ContinueUserActivity(UIApplication *application,
                                             NSUserActivity *userActivity,
                                             void (^restorationHandler)(NSArray *)) {
  if (s_invites_ios_startups) {
    for (auto it = s_invites_ios_startups->begin(); it != s_invites_ios_startups->end(); ++it) {
      if ((*it)->HandleContinueUserActivity(application, userActivity, restorationHandler)) {
        return YES;
      }
    }
  }
  return NO;
}

}  // namespace internal

}  // namespace invites
}  // namespace firebase

// Category for UIApplication that is used to hook methods in all classes.  Category +load() methods
// are called after all class load methods in each Mach-O (see call_load_methods() in
// http://www.opensource.apple.com/source/objc4/objc4-274/runtime/objc-runtime.m)
@implementation UIApplication (FIRFBI)
+ (void)load {
  ::firebase::LogDebug("Loading UIApplication FIRFBI category");
  ::firebase::util::ForEachAppDelegateClass(^(Class clazz) {
    ::firebase::invites::HookAppDelegateMethods(clazz);
  });
}
@end
