// Copyright 2016 Google LLC
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

#include "messaging/src/include/firebase/messaging.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>
#import <objc/runtime.h>

#include <map>
#include <memory.h>
#include <string>
#include <unordered_set>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/util_ios.h"
#include "messaging/src/common.h"

#import "FIRMessaging.h"

// This implements the messaging protocol so that we can receive notifcatons from Messaging.
NS_ASSUME_NONNULL_BEGIN
@interface FIRCppDelegate : NSObject<FIRMessagingDelegate>

// Set to YES when Messaging C++ has a listener set.
@property(nonatomic, readwrite) BOOL isListenerSet;
// Cached properties, if there is no listener set when
// messaging:didReceiveRegistrationToken: is called.
@property(nonatomic, readwrite, nullable) FIRMessaging *cachedMessaging;
@property(nonatomic, readwrite, nullable) NSString *cachedFCMToken;

// If a listener is set, this will notify Messaging of the token as normal.
// If no listener is set yet, the data will be cached until one is.
// NOLINTNEXTLINE
- (void)messaging:(FIRMessaging *)messaging didReceiveRegistrationToken:(nullable NSString *)FCMToken;

// Once the listener is registered, process cached token (if there is one).
// This will call messaging:didReceiveRegistrationToken:, passing in the cached values:
// cachedMessaging and cachedFCMToken.
- (void)processCachedRegistrationToken;
@end
NS_ASSUME_NONNULL_END

// The subscribe and unsubscribe methods in iOS run a completion block if the topic name is valid,
// but do nothing if the topic name is invalid (neither return an error code nor run the supplied
// completion). This means we have no way of knowing whether the future we create and pass to our
// completion block will ever complete. To work around this, we expose normalizeTopic, which is the
// function used to validate and normalize the given topic string. If this returns nil, it was an
// invalid topic name, and we know to complete the future immediately with the proper error code.
@interface FIRMessaging (NormalizeTopic)
+ (NSString *)normalizeTopic:(NSString *)topic;
@end

namespace firebase {
namespace messaging {

DEFINE_FIREBASE_VERSION_STRING(FirebaseMessaging);

static FIRCppDelegate *g_delegate = nil;
static Mutex g_delegate_mutex;  // Mutex for g_delegate's properties
static MessagingOptions g_messaging_options;

static bool MessagingIsInitialized() {
  MutexLock lock(g_delegate_mutex);
  return g_delegate.isListenerSet;
}

static NSString *const kReservedPrefix = @"google.";
static NSString *const kGcmPrefix = @"gcm.";

// Message keys.
static NSString *const kFrom = @"from";
static NSString *const kTo = @"to";
static NSString *const kCollapseKey = @"collapse_key";
static NSString *const kRawData = @"rawData";
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
static NSString *const kLink = @"gcm.n.link";

// Dual purpose body text or data dictionary.
static NSString *const kAlert = @"alert";

static void HookAppDelegateMethods(Class clazz);
static void NotifyApplicationAndServiceOfMessage(NSDictionary *user_info);

// Global reference to the Firebase App.
static const ::firebase::App *g_app = nullptr;

static id<UNUserNotificationCenterDelegate> g_user_delegate = nil;

// Caches the notification used to launch the application.  This is delivered when the API is
// initialized.
static NSDictionary *g_launch_notification;

static bool g_message_notification_opened = false;

static SafeFutureHandle<void> g_permission_prompt_future_handle;

static void RetrieveRegistrationToken();

static ::firebase::util::ClassMethodImplementationCache& SwizzledMethodCache() {
  static ::firebase::util::ClassMethodImplementationCache *g_swizzled_method_cache;
  return *::firebase::util::ClassMethodImplementationCache::GetCreateCache(
       &g_swizzled_method_cache);
}

InitResult Initialize(const ::firebase::App &app, Listener *listener) {
  return Initialize(app, listener, MessagingOptions());
}

InitResult Initialize(
    const ::firebase::App &app, Listener *listener, const MessagingOptions& options) {
  if (!g_delegate) {
    g_delegate = [[FIRCppDelegate alloc] init];
    [FIRMessaging messaging].delegate = g_delegate;
  }

  if (!g_app) {
#if !defined(NDEBUG)
    {
      MutexLock lock(g_delegate_mutex);
      assert(!g_delegate.isListenerSet);
    }
#endif  // !defined(NDEBUG)
    g_app = &app;
  } else {
    LogError("Messaging already initialized.");
    return kInitResultSuccess;
  }
  FutureData::Create();
  g_messaging_options = options;
  SetListenerIfNotNull(listener);
  internal::RegisterTerminateOnDefaultAppDestroy();
  return kInitResultSuccess;
}

// If listener is set, notify the user of any registration tokens or messages.
void NotifyListenerSet(Listener* listener) {
  // If no listener is specified, or the API is already initialized, do nothing.
  if (!listener) return;
  {
    MutexLock lock(g_delegate_mutex);
    if (g_delegate.isListenerSet) return;
    g_delegate.isListenerSet = YES;
  }

  if (!g_messaging_options.suppress_notification_permission_prompt) {
    RequestPermission();
  }
}

Future<void> RequestPermission() {
  FIREBASE_ASSERT_RETURN(RequestPermissionLastResult(), internal::IsInitialized());

  if (RequestPermissionLastResult().status() == kFutureStatusPending) {
    LogError("Status is pending. Return the pending future.");
    return RequestPermissionLastResult();
  }

  // Create the future.
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  g_permission_prompt_future_handle =
      api->SafeAlloc<void>(kMessagingFnRequestPermission);
  // Run on UI thread.
  dispatch_async(dispatch_get_main_queue(), ^{
    LogInfo("FCM: Initialize Firebase Messaging");

    NSString *senderID = @(firebase::App::GetInstance()->options().messaging_sender_id());
    LogInfo("FCM: Using FCM senderID %s", senderID.UTF8String);
    id appDelegate = [UIApplication sharedApplication];

    // Register for remote notifications. Both codepaths result in
    // application:didRegisterForRemoteNotificationsWithDeviceToken: being called when they
    // complete, or application:didFailToRegisterForRemoteNotificationsWithError: if there was an
    // error. We complete the future there.
    if (floor(NSFoundationVersionNumber) <= NSFoundationVersionNumber_iOS_7_1) {
      // iOS 7.1 or earlier
      UIRemoteNotificationType allNotificationTypes =
          (UIRemoteNotificationTypeSound | UIRemoteNotificationTypeAlert |
           UIRemoteNotificationTypeBadge);
      [appDelegate registerForRemoteNotificationTypes:allNotificationTypes];
    } else {
      // iOS 8 or later
      UIUserNotificationType allNotificationTypes =
          (UIUserNotificationTypeSound | UIUserNotificationTypeAlert | UIUserNotificationTypeBadge);
      UIUserNotificationSettings *settings =
          [UIUserNotificationSettings settingsForTypes:allNotificationTypes categories:nil];
      [appDelegate registerUserNotificationSettings:settings];
      [appDelegate registerForRemoteNotifications];
    }

    // Only request the token automatically if permitted
    if ([FIRMessaging messaging].autoInitEnabled) {
      [g_delegate processCachedRegistrationToken];
      RetrieveRegistrationToken();
    }

    // Notify the application of the launch notification.
    if (g_launch_notification != nil) {
      NotifyApplicationAndServiceOfMessage(g_launch_notification);
      g_launch_notification = nil;
    }
  });
  return MakeFuture(api, g_permission_prompt_future_handle);
}

Future<void> RequestPermissionLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnRequestPermission));
}

namespace internal {

// As opposed to MessagingIsInitialized() which determines whether FIRMessaging is configured, this
// returns whether the C++ has been initialized by the developer.
bool IsInitialized() {
  return g_app != nullptr;
}

}  // namespace internal

void Terminate() {
  if (!g_app) {
    LogError("Messaging already shut down.");
    return;
  }
  FutureData::Destroy();
  internal::UnregisterTerminateOnDefaultAppDestroy();
  // Ensure g_delegate still exists (it cannot be unallocated in case a token is received via a
  // different Firebase library e.g. Invites while Messaging is not enabled).
  {
    MutexLock lock(g_delegate_mutex);
    g_delegate.isListenerSet = NO;
    g_delegate.cachedMessaging = nil;
    g_delegate.cachedFCMToken = nil;
  }
  SetListener(nullptr);
  g_app = nullptr;
}

// Reconnect to FCM when an app returns to the foreground.
static void AppDelegateApplicationDidBecomeActive(id self, SEL selector_value,
                                                  UIApplication *application) {
  IMP app_delegate_application_did_become_active =
      SwizzledMethodCache().GetMethodForObject(self, @selector(applicationDidBecomeActive:));
  if (app_delegate_application_did_become_active) {
    ((util::AppDelegateApplicationDidBecomeActiveFunc)app_delegate_application_did_become_active)(
        self, selector_value, application);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector_value];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [self forwardInvocation:invocation];
  }
}

// Disconnect from FCM when an app is moved to the background.
static void AppDelegateApplicationDidEnterBackground(id self, SEL selector_value,
                                                     UIApplication *application) {
  IMP app_delegate_application_did_enter_background =
      SwizzledMethodCache().GetMethodForObject(self, @selector(applicationDidEnterBackground:));
  if (app_delegate_application_did_enter_background) {
    ((util::AppDelegateApplicationDidEnterBackgroundFunc)
         app_delegate_application_did_enter_background)(self, selector_value, application);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector_value];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [self forwardInvocation:invocation];
  }
}

// Register the device with FCM and retrieve a new registration token.
static void RetrieveRegistrationToken() {
  LogInfo("FCM: Retrieve registration token");
  NSString *registration_token = [[FIRMessaging messaging] FCMToken];
  // Check if there is a registration token. If there isn't one, request that one be generated, then
  // call back into this function.
  if (registration_token.length) {
    NotifyListenerOnTokenReceived(registration_token.UTF8String);
  }
}

static void CompletePermissionFuture(::firebase::messaging::Error error) {
  if (internal::IsInitialized()) {
    ::firebase::ReferenceCountedFutureImpl* api = ::firebase::messaging::FutureData::Get()->api();
    // The ReferenceCountedFutureImpl object should be initialized in Initialize().
    FIREBASE_ASSERT_RETURN_VOID(api != nullptr);
    if (api->ValidFuture(::firebase::messaging::g_permission_prompt_future_handle) &&
        RequestPermissionLastResult().status() == kFutureStatusPending) {
      api->Complete(::firebase::messaging::g_permission_prompt_future_handle, error);
    }
  } else {
    LogError("Attempting to complete future before FCM initialized.");
  }
}

static void AppDelegateApplicationDidRegisterForRemoteNotificationsWithDeviceToken(
    id self, SEL selector_value, UIApplication *application, NSData *deviceToken) {
  ::firebase::LogDebug("FCM: Registered for remote notifications with device token");
  CompletePermissionFuture(::firebase::messaging::kErrorNone);
  IMP app_delegate_application_did_register_for_remote_notifications_with_device_token =
      SwizzledMethodCache().GetMethodForObject(
          self, @selector(application:didRegisterForRemoteNotificationsWithDeviceToken:));
  if (app_delegate_application_did_register_for_remote_notifications_with_device_token) {
    ((util::AppDelegateApplicationDidRegisterForRemoteNotificationsWithDeviceTokenFunc)
         app_delegate_application_did_register_for_remote_notifications_with_device_token)(
        self, selector_value, application, deviceToken);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector_value];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&deviceToken atIndex:3];
    [self forwardInvocation:invocation];
  }
}


// Captures any errors that occur if remote notification registration fails.
static void AppDelegateApplicationDidFailToRegisterForRemoteNotificationsWithError(
    id self, SEL selector_value, UIApplication *application, NSError *error) {
  LogError("Failed to register for remote notifications.");
  CompletePermissionFuture(::firebase::messaging::kErrorFailedToRegisterForRemoteNotifications);
  // TODO(smiles): Report this error to the user.
  IMP app_delegate_application_did_fail_to_register_for_remote_notifications_with_error =
      SwizzledMethodCache().GetMethodForObject(
          self, @selector(application:didFailToRegisterForRemoteNotificationsWithError:));
  if (app_delegate_application_did_fail_to_register_for_remote_notifications_with_error) {
    ((util::AppDelegateApplicationDidFailToRegisterForRemoteNotificationsWithErrorFunc)
         app_delegate_application_did_fail_to_register_for_remote_notifications_with_error)(
        self, selector_value, application, error);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector_value];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&error atIndex:3];
    [self forwardInvocation:invocation];
  }
}

// Query the specified dictionary for a string key, if it's found an STL string is returned
// otherwise if dictionary is nil or the key isn't found this returns an empty string.
static std::string NSDictionaryGetString(const NSDictionary *dictionary, NSString *key) {
  std::string string_value;
  if (dictionary != nil) {
    id value = [dictionary objectForKey:key];
    if (value != nil && [value isKindOfClass:[NSString class]]) {
      string_value = ((NSString *)value).UTF8String;
    }
  }
  return string_value;
}


static std::vector<uint8_t> NSDictionaryGetByteVector(
    const NSDictionary *dictionary, NSString *key) {
  std::vector<uint8_t> result;
  if (dictionary != nil) {
    id value = [dictionary objectForKey:key];
    if (value != nil && [value isKindOfClass:[NSData class]]) {
      NSData *data = (NSData *)value;
      NSUInteger length = data.length;
      result.resize(static_cast<size_t>(length));
      [data getBytes:result.data() length:length];
    }
  }
  return result;
}

// Query the specified dictionary for a dictionary matching a key, if the key isn't found or
// the dictionary is nil this returns nil.
static NSDictionary *NSDictionaryGetDictionary(const NSDictionary *dictionary, NSString *key) {
  if (dictionary != nil) {
    id value = [dictionary objectForKey:key];
    if (value != nil && [value isKindOfClass:[NSDictionary class]]) {
      return (NSDictionary *)value;
    }
  }
  return nil;
}

// Get an integer value from a dictionary.
static int32_t NSDictionaryGetInteger(const NSDictionary *dictionary, NSString *key) {
  id value = dictionary[key];
  if ([value isKindOfClass:[NSNumber class]]) {
    return [value intValue];
  }
  return 0;
}

// Returns true if the given key string is not a reserved value. Reserved values are keys that start
// with "gcm." or "google.", or which are one of a handful of specific strings.
static bool IsUnreservedKey(NSString *key) {
  if ([key hasPrefix:kReservedPrefix] || [key hasPrefix:kGcmPrefix]) {
    return false;
  }
  static NSString *const reserved_keys[] = {kFrom, kTo, kCollapseKey, kRawData, kMessageID,
                                            kMessageType, kPriority, kTimeToLive, kError,
                                            kErrorDescription};
  for (int i = 0; i < FIREBASE_ARRAYSIZE(reserved_keys); ++i) {
    if ([key isEqualToString:reserved_keys[i]]) {
      return false;
    }
  }
  return true;
}

// Convert a dictionary of strings indexed by strings to a string map.  All non-string keys and
// values are ignored.
static void NSDictionaryToStringMap(const NSDictionary *dictionary,
                                    std::map<std::string, std::string> *string_map) {
  [dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
    if ([key isKindOfClass:[NSString class]] && IsUnreservedKey(key) &&
        [obj isKindOfClass:[NSString class]]) {
      (*string_map)[((NSString *)key).UTF8String] = ((NSString *)obj).UTF8String;
    }
  }];
}

// Notify the application of the message and the service.
static void NotifyApplicationAndServiceOfMessage(NSDictionary *user_info) {
  Message message;
  Notification notification;

  // Parse aps: { alert: $payload } where $payload can be a dictionary or string
  // See https://developer.apple.com/library/ios/documentation/NetworkingInternet/Conceptual/
  // RemoteNotificationsPG/Chapters/TheNotificationPayload.html
  NSDictionary *aps = NSDictionaryGetDictionary(user_info, @"aps");
  if (aps != nil) {
    message.notification = &notification;
    NSDictionary<NSString *, NSString *> *alert_dictionary = NSDictionaryGetDictionary(aps, kAlert);
    if (alert_dictionary != nil) {
      notification.title = NSDictionaryGetString(alert_dictionary, kTitle);
      notification.body = NSDictionaryGetString(alert_dictionary, kBody);
    } else {
      std::string alert_string = NSDictionaryGetString(aps, kAlert);
      if (alert_string.length()) {
        notification.body = alert_string;
      }
    }
    notification.sound = NSDictionaryGetString(aps, kSound);
    notification.badge = NSDictionaryGetString(aps, kBadge);
  }

  message.from = NSDictionaryGetString(user_info, kFrom);
  message.to = NSDictionaryGetString(user_info, kTo);
  message.collapse_key = NSDictionaryGetString(user_info, kCollapseKey);
  message.message_id = NSDictionaryGetString(user_info, kMessageID);
  message.message_type = NSDictionaryGetString(user_info, kMessageType);
  message.priority = NSDictionaryGetString(user_info, kPriority);
  message.time_to_live = NSDictionaryGetInteger(user_info, kTimeToLive);
  message.error = NSDictionaryGetString(user_info, kError);
  message.error_description = NSDictionaryGetString(user_info, kErrorDescription);
  NSDictionaryToStringMap(user_info, &message.data);
  message.raw_data = NSDictionaryGetByteVector(user_info, kRawData);
  message.notification_opened = g_message_notification_opened;
  message.link = NSDictionaryGetString(user_info, kLink);
  g_message_notification_opened = false;
  // TODO(smiles): Do we need to handle binary / raw payloads?
  if (HasListener()) {
    NotifyListenerOnMessage(message);
    [[FIRMessaging messaging] appDidReceiveMessage:user_info];
  }
  message.notification = nullptr;
}

static BOOL AppDelegateApplicationDidFinishLaunchingWithOptions(id self, SEL selector_value,
                                                                UIApplication *application,
                                                                NSDictionary *launch_options) {
  // Set up Messaging on iOS 10, if possible.
  Class notification_center_class = NSClassFromString(@"UNUserNotificationCenter");
  if (notification_center_class && application) {
    LogInfo("Setting up iOS 10 message delegate.");

    // Cache the existing delegate if one exists it so we can pass along messages when needed.
    id user_notification_center = [notification_center_class currentNotificationCenter];
    g_user_delegate = [user_notification_center delegate];

    // Because we've added the methods, we know that `application` supports the
    // UNUserNotificationCenterDelegate protocol.
    [user_notification_center setDelegate:(id<UNUserNotificationCenterDelegate>)application];
  }

  // If the app was launched with a notification, cache it until we're connected.
  g_launch_notification =
      [launch_options objectForKey:UIApplicationLaunchOptionsRemoteNotificationKey];
  g_message_notification_opened = g_launch_notification != nil;

  IMP app_delegate_application_did_finish_launching_with_options =
      SwizzledMethodCache().GetMethodForObject(
          self, @selector(application:didFinishLaunchingWithOptions:));
  if (app_delegate_application_did_finish_launching_with_options) {
    return ((util::AppDelegateApplicationDidFinishLaunchingWithOptionsFunc)
                app_delegate_application_did_finish_launching_with_options)(
        self, selector_value, application, launch_options);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector_value];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&launch_options atIndex:3];
    [self forwardInvocation:invocation];
    // Read the return value from the invocation.
    BOOL ret = NO;
    [invocation getReturnValue:&ret];
    return ret;
  }
  return NO;
}

// Intercepts a notification (message).
static void AppDelegateApplicationDidReceiveRemoteNotification(id self, SEL selector_value,
                                                               UIApplication *application,
                                                               NSDictionary *user_info) {
  g_message_notification_opened = (application.applicationState == UIApplicationStateInactive ||
                                   application.applicationState == UIApplicationStateBackground);
#if !defined(NDEBUG)
  LogInfo("FCM: Received notification (no handler): %s", [user_info description].UTF8String);
#else
  LogInfo("FCM: Received notification (no handler)");
#endif

  if (MessagingIsInitialized()) {
    NotifyApplicationAndServiceOfMessage(user_info);
  }
  IMP app_delegate_application_did_receive_remote_notification =
      SwizzledMethodCache().GetMethodForObject(
          self, @selector(application:didReceiveRemoteNotification:));
  if (app_delegate_application_did_receive_remote_notification) {
    ((util::AppDelegateApplicationDidReceiveRemoteNotificationFunc)
         app_delegate_application_did_receive_remote_notification)(self, selector_value,
                                                                   application, user_info);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector_value];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&user_info atIndex:3];
    [self forwardInvocation:invocation];
  }
}

// Intercepts a notification (message), provides a completion handler which is called when the
// notification payload has been downloaded.
static void AppDelegateApplicationDidReceiveRemoteNotificationFetchCompletionHandler(
    id self, SEL selector_value, UIApplication *application, NSDictionary *user_info,
    util::UIBackgroundFetchResultFunction handler) {
  g_message_notification_opened = (application.applicationState == UIApplicationStateInactive ||
                                   application.applicationState == UIApplicationStateBackground);
#if !defined(NDEBUG)
  LogInfo("FCM: Received notification (using handler): %s", [user_info description].UTF8String);
#else
  LogInfo("FCM: Received notification (using handler)");
#endif
  if (MessagingIsInitialized()) {
    NotifyApplicationAndServiceOfMessage(user_info);
  }
  IMP app_delegate_application_did_receive_remote_notification_fetch_completion_handler =
      SwizzledMethodCache().GetMethodForObject(
          self, @selector(application:didReceiveRemoteNotification:fetchCompletionHandler:));
  if (app_delegate_application_did_receive_remote_notification_fetch_completion_handler) {
    ((util::AppDelegateApplicationDidReceiveRemoteNotificationFetchCompletionHandlerFunc)
         app_delegate_application_did_receive_remote_notification_fetch_completion_handler)(
             self, selector_value, application, user_info, handler);
  } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
             [NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
    NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector_value];
    [invocation setTarget:self];
    [invocation setArgument:&application atIndex:2];
    [invocation setArgument:&user_info atIndex:3];
    [invocation setArgument:&handler atIndex:4];
    [self forwardInvocation:invocation];
  } else {
    // TODO(smiles): We should determine whether the entire message is sent to this notification
    // method, if not this is clearly wrong and will need to download the rest of the message.
    // This is conditional based upon whether the user has implemented this method so they can
    // callback the handler when they've completed a background fetch.
    handler(UIBackgroundFetchResultNoData);
  }
}

// Hook all AppDelegate methods that FCM requires to intercept messaging events.
//
// The user of the library provides the class which implements AppDelegate protocol so this
// method hooks methods of user's UIApplication class in order to intercept events required for
// FCM.  The alternative to this procedure would require the user to implement boilerplate code
// in their UIApplication in order to plumb in FCM.
//
// The following methods are replaced in order to intercept AppDelegate events:
// * applicationDidBecomeActive:
//   Reconnect to FCM when an app returns to the foreground.
// * applicationDidEnterBackground:
//   Disconnect from FCM when an app is moved to the background.
// * application:didFailToRegisterForRemoteNotificationsWithError:
//   Captures any errors that occur if remote notification registration fails.
// * application:didReceiveRemoteNotification:
//   Intercepts a notification (message).
//   NOTE: This should never be called since we've implemented the fetchCompletionHandler variant.
// * application:didReceiveRemoteNotification:fetchCompletionHandler:
//   Intercepts a notification (message), provides a completion handler which is called when the
//   notification payload has been downloaded.
static void HookAppDelegateMethods(Class clazz) {
  Class method_encoding_class = [FIRSAMAppDelegate class];
  auto& method_cache = SwizzledMethodCache();
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(application:didFinishLaunchingWithOptions:),
      (IMP)AppDelegateApplicationDidFinishLaunchingWithOptions, method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(applicationDidBecomeActive:), (IMP)AppDelegateApplicationDidBecomeActive,
      method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(applicationDidEnterBackground:),
      (IMP)AppDelegateApplicationDidEnterBackground, method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(application:didRegisterForRemoteNotificationsWithDeviceToken:),
      (IMP)AppDelegateApplicationDidRegisterForRemoteNotificationsWithDeviceToken,
      method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(application:didFailToRegisterForRemoteNotificationsWithError:),
      (IMP)AppDelegateApplicationDidFailToRegisterForRemoteNotificationsWithError,
      method_encoding_class);
  method_cache.ReplaceOrAddMethod(
      clazz, @selector(application:didReceiveRemoteNotification:),
      (IMP)AppDelegateApplicationDidReceiveRemoteNotification, method_encoding_class);
  // application:didReceiveRemoteNotification:fetchCompletionHandler: is called in preference to
  // application:didReceiveRemoteNotification: so if the UIApplicationDelegate does not implement
  // application:didReceiveRemoteNotification:fetchCompletionHandler: do not hook it.
  method_cache.ReplaceMethod(
      clazz, @selector(application:didReceiveRemoteNotification:fetchCompletionHandler:),
      (IMP)AppDelegateApplicationDidReceiveRemoteNotificationFetchCompletionHandler,
      method_encoding_class);
}

static const char kErrorMessageNoRegistrationToken[] =
    "Cannot update subscritption when SetTokenRegistrationOnInitEnabled is set to false";

Future<void> Subscribe(const char *topic) {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());

  // Create the future.
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnSubscribe);

  LogInfo("FCM: Subscribe to topic `%s`", topic);

  if (!IsTokenRegistrationOnInitEnabled()) {
    api->Complete(handle, kErrorNoRegistrationToken,
                  kErrorMessageNoRegistrationToken);
    return MakeFuture(api, handle);
  }
  if (![FIRMessaging normalizeTopic:@(topic)]) {
    std::string error = "Cannot parse topic name ";
    error += topic;
    error += ". Will not subscribe.";
    api->Complete(handle, kErrorInvalidTopicName, error.c_str());
    return MakeFuture(api, handle);
  }
  [[FIRMessaging messaging] subscribeToTopic:@(topic)
                                  completion:^void(NSError *error) {
        if (error) {
          api->Complete(handle, kErrorInvalidTopicName, error.localizedDescription.UTF8String);
        } else {
          api->Complete(handle, kErrorNone);
        }
      }];
  return MakeFuture(api, handle);
}

Future<void> SubscribeLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnSubscribe));
}

Future<void> Unsubscribe(const char *topic) {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());

  // Create the future.
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnUnsubscribe);

  LogInfo("FCM: Unsubscribe to topic `%s`", topic);

  if (!IsTokenRegistrationOnInitEnabled()) {
    api->Complete(handle, kErrorNoRegistrationToken,
                  kErrorMessageNoRegistrationToken);
    return MakeFuture(api, handle);
  }
  if (![FIRMessaging normalizeTopic:@(topic)]) {
    std::string error = "Cannot parse topic name ";
    error += topic;
    error += ". Will not unsubscribe.";
    api->Complete(handle, kErrorInvalidTopicName, error.c_str());
    return MakeFuture(api, handle);
  }
  [[FIRMessaging messaging] unsubscribeFromTopic:@(topic)
                                      completion:^void(NSError *error) {
        if (error) {
          api->Complete(handle, kErrorInvalidTopicName, error.localizedDescription.UTF8String);
        } else {
          api->Complete(handle, kErrorNone);
        }
      }];
  return MakeFuture(api, handle);
}

Future<void> UnsubscribeLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnUnsubscribe));
}

bool DeliveryMetricsExportToBigQueryEnabled() {
  // TODO(146362498): Implement this once the underlying API is ready on iOS.
  LogWarning("DeliveryMetricsExportToBigQueryEnabled is not currently implemented on iOS");

  return false;
}

void SetDeliveryMetricsExportToBigQuery(bool /*enable*/) {
  // TODO(146362498): Implement this once the underlying API is ready on iOS.
  LogWarning("SetDeliveryMetricsExportToBigQuery is not currently implemented on iOS");
}

bool IsTokenRegistrationOnInitEnabled() { return [FIRMessaging messaging].autoInitEnabled; }

void SetTokenRegistrationOnInitEnabled(bool enable) {
  bool was_enabled = IsTokenRegistrationOnInitEnabled();
  [FIRMessaging messaging].autoInitEnabled = enable;
  // TODO(b/77662386): This shouldn't be required, but the native API
  // doesn't raise the event when flipping the bit to true, so we watch for
  // that here.
  if (!was_enabled && IsTokenRegistrationOnInitEnabled()) {
    [g_delegate processCachedRegistrationToken];
    RetrieveRegistrationToken();
  }
}

Future<std::string> GetToken() {
  FIREBASE_ASSERT_RETURN(GetTokenLastResult(), internal::IsInitialized());

  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<std::string> handle =
      api->SafeAlloc<std::string>(kMessagingFnGetToken);

  [[FIRMessaging messaging] tokenWithCompletion:^(NSString *_Nullable token,
                                           NSError *_Nullable error) {
    if (error) {
      api->Complete(handle, kErrorUnknown,
                    util::NSStringToString(error.localizedDescription).c_str());
    } else {
      api->CompleteWithResult(handle, kErrorNone,
                              "", util::NSStringToString(token));
    }
  }];

  return MakeFuture(api, handle);
}

Future<std::string> GetTokenLastResult() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<std::string>&>(
      api->LastResult(kMessagingFnGetToken));
}

Future<void> DeleteToken() {
  FIREBASE_ASSERT_RETURN(DeleteTokenLastResult(), internal::IsInitialized());

  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnDeleteToken);

  [[FIRMessaging messaging] deleteTokenWithCompletion:^(NSError *_Nullable error) {
    api->Complete(handle,
                  error == nullptr ? kErrorNone : kErrorUnknown,
                  util::NSStringToString(error.localizedDescription).c_str());
  }];

  return MakeFuture(api, handle);
}

Future<void> DeleteTokenLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnDeleteToken));
}

}  // namespace messaging
}  // namespace firebase

extern "C" void FirebaseMessagingHookAppDelegate(Class app_delegate) {
  ::firebase::messaging::HookAppDelegateMethods(app_delegate);
}

// Category for UIApplication that is used to hook methods in all classes.
// Category +load() methods are called after all class load methods in each Mach-O
// (see call_load_methods() in
// http://www.opensource.apple.com/source/objc4/objc4-274/runtime/objc-runtime.m)
@implementation UIApplication (FIRFCM)
+ (void)load {
  ::firebase::LogInfo("FCM: Loading UIApplication FIRFCM category");
  ::firebase::util::ForEachAppDelegateClass(^(Class clazz) {
    FirebaseMessagingHookAppDelegate(clazz);
  });
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)notificationCenter
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))completionHandler {
  ::firebase::LogInfo("FCM: Received notification through notification center.");
  if (::firebase::messaging::MessagingIsInitialized()) {
    NSDictionary *userInfo = [[[notification request] content] userInfo];

#if !defined(NDEBUG)
    ::firebase::LogInfo("FCM: userInfo: %s.", userInfo.description.UTF8String);
#endif
    ::firebase::messaging::g_message_notification_opened = false;
    ::firebase::messaging::NotifyApplicationAndServiceOfMessage(userInfo);
    id<UNUserNotificationCenterDelegate> user_delegate = ::firebase::messaging::g_user_delegate;
    [user_delegate userNotificationCenter:notificationCenter
                  willPresentNotification:notification
                    withCompletionHandler:completionHandler];
  }
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)notificationCenter
didReceiveNotificationResponse:(UNNotificationResponse *)response
         withCompletionHandler:(void (^)())completionHandler {
  if (::firebase::messaging::MessagingIsInitialized()) {
    NSDictionary *userInfo = [[[[response notification] request] content] userInfo];
#if !defined(NDEBUG)
    ::firebase::LogInfo("FCM: userInfo: %s.", userInfo.description.UTF8String);
#endif
    ::firebase::messaging::g_message_notification_opened = true;
    ::firebase::messaging::NotifyApplicationAndServiceOfMessage(userInfo);
    id<UNUserNotificationCenterDelegate> user_delegate = ::firebase::messaging::g_user_delegate;
    [user_delegate userNotificationCenter:notificationCenter
           didReceiveNotificationResponse:response
                    withCompletionHandler:completionHandler];
  }
}

@end

@implementation FIRCppDelegate
- (void)messaging:(FIRMessaging *)messaging didReceiveRegistrationToken:(NSString *)fcmToken {
  ::firebase::messaging::g_delegate_mutex.Acquire();
  if (_isListenerSet) {
    ::firebase::messaging::g_delegate_mutex.Release();
    ::firebase::LogInfo("FCM: new registration token received.");
    ::firebase::messaging::RetrieveRegistrationToken();
  } else {
    _cachedMessaging = messaging;
    _cachedFCMToken = fcmToken;
    ::firebase::messaging::g_delegate_mutex.Release();
    ::firebase::LogInfo(
        "FCM: registration token received, but no listener set yet - cached the token.");
  }
}

- (void)processCachedRegistrationToken {
  FIRMessaging *msg;
  NSString *token;
  {
    firebase::MutexLock lock(::firebase::messaging::g_delegate_mutex);
    // TODO(butterfield): What if there's no cached message but there is a cached token?
    // The user may never get notified of the old token before a new one is registered.
    if (!_isListenerSet || !_cachedMessaging || !_cachedFCMToken) {
      return;
    }
    msg = _cachedMessaging;
    token = _cachedFCMToken;
    _cachedFCMToken = nil;
    _cachedMessaging = nil;
  }
  [self messaging:msg didReceiveRegistrationToken:token];
}
@end
