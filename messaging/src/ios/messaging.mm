#include <cassert>
#include <cstdint>
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
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util_ios.h"
#include "messaging/src/common.h"
#import "FIRMessaging.h"
NS_ASSUME_NONNULL_BEGIN @interface FIRCppDelegate : NSObject<FIRMessagingDelegate>
@property(nonatomic, readwrite) BOOL isListenerSet;
@property(nonatomic, readwrite, nullable) FIRMessaging *cachedMessaging;
@property(nonatomic, copy, nullable) NSString *cachedFCMToken;
@property(nonatomic, copy, nullable) NSString *cachedRegistrationId;
- (void)messaging:(FIRMessaging *)messaging didReceiveRegistrationToken:(nullable NSString *)FCMToken;
- (void)messaging:(FIRMessaging *)messaging didReceiveRegistration:(nullable NSString *)installationId;
- (void)messaging:(FIRMessaging *)messaging didUnregister:(nullable NSString *)installationId;
- (BOOL)processCachedRegistrationToken; @end NS_ASSUME_NONNULL_END @interface FIRMessaging (NormalizeTopic)
+ (NSString *)normalizeTopic:(NSString *)topic; @end namespace firebase { namespace messaging {
DEFINE_FIREBASE_VERSION_STRING(FirebaseMessaging); static FIRCppDelegate *g_delegate = nil;
static Mutex g_delegate_mutex; static MessagingOptions g_messaging_options; static bool MessagingIsInitialized() {
MutexLock lock(g_delegate_mutex); return g_delegate.isListenerSet; }
static NSString *const kReservedPrefix = @"google."; static NSString *const kGcmPrefix = @"gcm.";
static NSString *const kFrom = @"from"; static NSString *const kTo = @"to";
static NSString *const kCollapseKey = @"collapse_key"; static NSString *const kRawData = @"rawData";
static NSString *const kMessageID = @"gcm.message_id"; static NSString *const kMessageType = @"message_type";
static NSString *const kPriority = @"priority"; static NSString *const kTimeToLive = @"time_to_live";
static NSString *const kError = @"error"; static NSString *const kErrorDescription = @"error_description";
static NSString *const kTitle = @"title"; static NSString *const kBody = @"body";
static NSString *const kSound = @"sound"; static NSString *const kBadge = @"badge";
static NSString *const kCategory = @"category"; static NSString *const kBodyLocKey = @"loc-key";
static NSString *const kBodyLocArgs = @"loc-args"; static NSString *const kTitleLocKey = @"title-loc-key";
static NSString *const kTitleLocArgs = @"title-loc-args"; static NSString *const kLink = @"gcm.n.link";
static NSString *const kAlert = @"alert"; static void HookAppDelegateMethods(Class clazz);
static void NotifyApplicationAndServiceOfMessage(NSDictionary *user_info, bool notification_opened = false);
static const ::firebase::App *g_app = nullptr; static id<UNUserNotificationCenterDelegate> g_user_delegate = nil;
static NSDictionary *g_launch_notification; static SafeFutureHandle<void> g_permission_prompt_future_handle;
static void RetrieveRegistrationToken();
static ::firebase::util::ClassMethodImplementationCache& SwizzledMethodCache() {
static ::firebase::util::ClassMethodImplementationCache *g_swizzled_method_cache;
return *::firebase::util::ClassMethodImplementationCache::GetCreateCache( &g_swizzled_method_cache); }
static void CompletePermissionFuture(::firebase::messaging::Error error,
const char* error_msg = nullptr) {  if (internal::IsInitialized() && FutureData::Get() && FutureData::Get()->api()) {
::firebase::ReferenceCountedFutureImpl* api = FutureData::Get()->api();
if (api->ValidFuture(g_permission_prompt_future_handle) &&
api->GetFutureStatus(g_permission_prompt_future_handle.get()) == kFutureStatusPending) { if (error_msg) {
api->Complete(g_permission_prompt_future_handle, error, error_msg); } else {
api->Complete(g_permission_prompt_future_handle, error); } } } else {
LogError("Attempting to complete future before FCM initialized."); } }
InitResult Initialize(const ::firebase::App &app, Listener *listener) {
return Initialize(app, listener, MessagingOptions()); } InitResult Initialize(
const ::firebase::App &app, Listener *listener, const MessagingOptions& options) { if (!g_delegate) {
g_delegate = [[FIRCppDelegate alloc] init]; [FIRMessaging messaging].delegate = g_delegate; }
bool already_initialized = false; { MutexLock lock(g_delegate_mutex); if (!g_app) { assert(!g_delegate.isListenerSet);
g_app = &app; } else { already_initialized = true; } } if (already_initialized) { SetListenerIfNotNull(listener);
LogError("Messaging already initialized."); return kInitResultSuccess; } {    FutureData::Create(); }
g_messaging_options = options; SetListenerIfNotNull(listener); internal::RegisterTerminateOnDefaultAppDestroy(app);
return kInitResultSuccess; } void NotifyListenerSet(Listener* listener) { if (!listener) return; {
MutexLock lock(g_delegate_mutex); if (g_delegate.isListenerSet) return; g_delegate.isListenerSet = YES; }
dispatch_async(dispatch_get_main_queue(), ^{ if (!internal::IsInitialized()) return;
id appDelegate = [UIApplication sharedApplication];
#if FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
if ([UNUserNotificationCenter class] != nil && appDelegate != nil) { UNUserNotificationCenter *center =
[UNUserNotificationCenter currentNotificationCenter]; if (center.delegate != nil && center.delegate != appDelegate) {
MutexLock lock(g_delegate_mutex); g_user_delegate = center.delegate; } center.delegate = appDelegate; }
#endif
if ([FIRMessaging messaging].autoInitEnabled) { if (![g_delegate processCachedRegistrationToken]) {
RetrieveRegistrationToken(); } } NSDictionary* cached_notification = nil; { MutexLock lock(g_delegate_mutex);
if (g_launch_notification != nil) { cached_notification = g_launch_notification; g_launch_notification = nil; } }
if (cached_notification != nil) { NotifyApplicationAndServiceOfMessage(cached_notification, true); } });
if (!g_messaging_options.suppress_notification_permission_prompt) { RequestPermission(); } }
Future<void> RequestPermission() {
return RunAsyncOp<void>(kMessagingFnRequestPermission, [](ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle) {
g_permission_prompt_future_handle = handle; dispatch_async(dispatch_get_main_queue(), ^{ std::string sender_id_str; {
MutexLock app_lock(g_delegate_mutex); if (!g_app) return; sender_id_str = g_app->options().messaging_sender_id(); }
LogInfo("FCM: Initialize Firebase Messaging"); NSString *senderID = @(sender_id_str.c_str());
LogInfo("FCM: Using FCM senderID %s", senderID.UTF8String); id appDelegate = [UIApplication sharedApplication];
#if FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
if ([UNUserNotificationCenter class] != nil) {
UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
if (center.delegate != nil && center.delegate != appDelegate) { MutexLock lock(g_delegate_mutex);
g_user_delegate = center.delegate; } center.delegate = appDelegate;
UNAuthorizationOptions authOptions = UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge;
[center requestAuthorizationWithOptions:authOptions completionHandler:^(BOOL granted, NSError * _Nullable error) {
if (error != nil || !granted) {
std::string err_str = error && error.localizedDescription.UTF8String ? error.localizedDescription.UTF8String : "Notification permission was not granted.";
CompletePermissionFuture(::firebase::messaging::kErrorFailedToRegisterForRemoteNotifications, err_str.c_str()); } else {
dispatch_async(dispatch_get_main_queue(), ^{
if ([appDelegate respondsToSelector:@selector(registerForRemoteNotifications)]) {
[appDelegate registerForRemoteNotifications]; } else { CompletePermissionFuture(::firebase::messaging::kErrorNone); }
}); } }]; } else { CompletePermissionFuture(::firebase::messaging::kErrorNone); }
#else
(void)appDelegate; CompletePermissionFuture(::firebase::messaging::kErrorNone);
#endif
}); }); } Future<void> RequestPermissionLastResult() { return LastResultFn<void>(kMessagingFnRequestPermission); }
namespace internal { bool IsInitialized() { MutexLock lock(g_delegate_mutex); return g_app != nullptr; } }
void Terminate() { { MutexLock lock(g_delegate_mutex); if (!g_app) { LogError("Messaging already shut down."); return; }
g_app = nullptr; g_delegate.isListenerSet = NO; g_delegate.cachedMessaging = nil; g_delegate.cachedFCMToken = nil;
g_delegate.cachedRegistrationId = nil; } internal::UnregisterTerminateOnDefaultAppDestroy(); {    FutureData::Destroy();
g_permission_prompt_future_handle = SafeFutureHandle<void>(); } SetListener(nullptr);
g_messaging_options = MessagingOptions(); id<UNUserNotificationCenterDelegate> user_delegate = nil; {
MutexLock lock(g_delegate_mutex); g_launch_notification = nil; user_delegate = g_user_delegate; g_user_delegate = nil; }
#if FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
if ([UNUserNotificationCenter class] != nil) { UNUserNotificationCenter *center =
[UNUserNotificationCenter currentNotificationCenter]; if (user_delegate != nil ||
center.delegate == (id<UNUserNotificationCenterDelegate>)[UIApplication sharedApplication]) {
center.delegate = user_delegate; } }
#endif }
static void RetrieveRegistrationToken() { LogInfo("FCM: Retrieve registration token");
NSString *registration_token = [[FIRMessaging messaging] FCMToken]; if (registration_token.length) {
NotifyListenerOnTokenReceived(registration_token.UTF8String); } }
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
self, selector_value, application, deviceToken); } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
[NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
[invocation setSelector:selector_value]; [invocation setTarget:self]; [invocation setArgument:&application atIndex:2];
[invocation setArgument:&deviceToken atIndex:3]; [self forwardInvocation:invocation]; } }
static void AppDelegateApplicationDidFailToRegisterForRemoteNotificationsWithError(
id self, SEL selector_value, UIApplication *application, NSError *error) {
LogError("Failed to register for remote notifications.");
CompletePermissionFuture(::firebase::messaging::kErrorFailedToRegisterForRemoteNotifications);
IMP app_delegate_application_did_fail_to_register_for_remote_notifications_with_error =
SwizzledMethodCache().GetMethodForObject(
self, @selector(application:didFailToRegisterForRemoteNotificationsWithError:));
if (app_delegate_application_did_fail_to_register_for_remote_notifications_with_error) {
((util::AppDelegateApplicationDidFailToRegisterForRemoteNotificationsWithErrorFunc)
app_delegate_application_did_fail_to_register_for_remote_notifications_with_error)(
self, selector_value, application, error); } else if ([self methodForSelector:@selector(forwardInvocation:)] !=
[NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
[invocation setSelector:selector_value]; [invocation setTarget:self]; [invocation setArgument:&application atIndex:2];
[invocation setArgument:&error atIndex:3]; [self forwardInvocation:invocation]; } }
static std::string NSDictionaryGetString(const NSDictionary *dictionary, NSString *key) { std::string string_value;
if (dictionary != nil) { id value = [dictionary objectForKey:key];
if (value != nil && [value isKindOfClass:[NSString class]]) { const char *utf8 = ((NSString *)value).UTF8String;
if (utf8) string_value = utf8; } } return string_value; }
static std::string NSDictionaryGetStringOrNumber(const NSDictionary *dictionary, NSString *key) {
std::string string_value; if (dictionary != nil) { id value = [dictionary objectForKey:key]; if (value != nil) {
if ([value isKindOfClass:[NSString class]]) { const char *utf8 = ((NSString *)value).UTF8String;
if (utf8) string_value = utf8; } else if ([value isKindOfClass:[NSNumber class]]) {
const char *utf8 = [((NSNumber *)value) stringValue].UTF8String; if (utf8) string_value = utf8; } } }
return string_value; } static std::vector<std::string> NSDictionaryGetStringVector(
const NSDictionary *dictionary, NSString *key) { std::vector<std::string> result; if (dictionary != nil) {
id value = [dictionary objectForKey:key]; if (value != nil && [value isKindOfClass:[NSArray class]]) {
NSArray *array = (NSArray *)value; for (id item in array) { if ([item isKindOfClass:[NSString class]]) {
const char *utf8 = ((NSString *)item).UTF8String; result.push_back(utf8 ? utf8 : "");
} else if ([item isKindOfClass:[NSNumber class]]) { const char *utf8 = [((NSNumber *)item) stringValue].UTF8String;
result.push_back(utf8 ? utf8 : ""); } } } } return result; } static std::vector<uint8_t> NSDictionaryGetByteVector(
const NSDictionary *dictionary, NSString *key) { std::vector<uint8_t> result; if (dictionary != nil) {
id value = [dictionary objectForKey:key]; if (value != nil && [value isKindOfClass:[NSData class]]) {
NSData *data = (NSData *)value; NSUInteger length = data.length; result.resize(static_cast<size_t>(length));
[data getBytes:result.data() length:length]; } } return result; }
static NSDictionary *NSDictionaryGetDictionary(const NSDictionary *dictionary, NSString *key) { if (dictionary != nil) {
id value = [dictionary objectForKey:key]; if (value != nil && [value isKindOfClass:[NSDictionary class]]) {
return (NSDictionary *)value; } } return nil; }
static int32_t NSDictionaryGetInteger(const NSDictionary *dictionary, NSString *key) { if (dictionary == nil) return 0;
id value = dictionary[key]; if ([value isKindOfClass:[NSNumber class]] || [value isKindOfClass:[NSString class]]) {
return [value intValue]; } return 0; } static bool IsUnreservedKey(NSString *key) {
if ([key hasPrefix:kReservedPrefix] || [key hasPrefix:kGcmPrefix]) { return false; }
static NSString *const reserved_keys[] = {kFrom, kTo, kCollapseKey, kRawData, kMessageID,
kMessageType, kPriority, kTimeToLive, kError, kErrorDescription, @"aps"};
for (size_t i = 0; i < FIREBASE_ARRAYSIZE(reserved_keys); ++i) { if ([key isEqualToString:reserved_keys[i]]) {
return false; } } return true; } static void NSDictionaryToStringMap(const NSDictionary *dictionary,
std::map<std::string, std::string> *string_map) {
[dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) { (void)stop;
if ([key isKindOfClass:[NSString class]] && IsUnreservedKey(key)) { const char *key_utf8 = ((NSString *)key).UTF8String;
if (!key_utf8) return; if ([obj isKindOfClass:[NSString class]]) { const char *val_utf8 = ((NSString *)obj).UTF8String;
if (val_utf8) (*string_map)[key_utf8] = val_utf8; } else if ([obj isKindOfClass:[NSNumber class]]) {
const char *val_utf8 = [((NSNumber *)obj) stringValue].UTF8String; if (val_utf8) (*string_map)[key_utf8] = val_utf8; } }
}]; } static void NotifyApplicationAndServiceOfMessage(NSDictionary *user_info, bool notification_opened) {
Message message; Notification notification; NSDictionary *aps = NSDictionaryGetDictionary(user_info, @"aps");
if (aps != nil && (aps[kAlert] != nil || aps[kSound] != nil || aps[kBadge] != nil)) {
message.notification = &notification; NSDictionary *alert_dictionary = NSDictionaryGetDictionary(aps, kAlert);
if (alert_dictionary != nil) { notification.title = NSDictionaryGetString(alert_dictionary, kTitle);
notification.body = NSDictionaryGetString(alert_dictionary, kBody); notification.body_loc_key =
NSDictionaryGetString(alert_dictionary, kBodyLocKey); notification.body_loc_args =
NSDictionaryGetStringVector(alert_dictionary, kBodyLocArgs); notification.title_loc_key =
NSDictionaryGetString(alert_dictionary, kTitleLocKey); notification.title_loc_args =
NSDictionaryGetStringVector(alert_dictionary, kTitleLocArgs); } else {
std::string alert_string = NSDictionaryGetString(aps, kAlert); if (alert_string.length()) {
notification.body = alert_string; } } notification.sound = NSDictionaryGetString(aps, kSound);
notification.badge = NSDictionaryGetStringOrNumber(aps, kBadge);
notification.click_action = NSDictionaryGetString(aps, kCategory); }
message.from = NSDictionaryGetString(user_info, kFrom); message.to = NSDictionaryGetString(user_info, kTo);
message.collapse_key = NSDictionaryGetString(user_info, kCollapseKey);
message.message_id = NSDictionaryGetString(user_info, kMessageID);
message.message_type = NSDictionaryGetString(user_info, kMessageType);
message.priority = NSDictionaryGetString(user_info, kPriority);
message.time_to_live = NSDictionaryGetInteger(user_info, kTimeToLive);
message.error = NSDictionaryGetString(user_info, kError);
message.error_description = NSDictionaryGetString(user_info, kErrorDescription);
NSDictionaryToStringMap(user_info, &message.data); message.raw_data = NSDictionaryGetByteVector(user_info, kRawData);
message.notification_opened = notification_opened; message.link = NSDictionaryGetString(user_info, kLink);
if (HasListener()) { NotifyListenerOnMessage(message); [[FIRMessaging messaging] appDidReceiveMessage:user_info]; }
message.notification = nullptr; }
static BOOL AppDelegateApplicationDidFinishLaunchingWithOptions(id self, SEL selector_value, UIApplication *application,
NSDictionary *launch_options) {
#if FIREBASE_PLATFORM_IOS {
MutexLock lock(g_delegate_mutex); g_launch_notification =
[launch_options objectForKey:UIApplicationLaunchOptionsRemoteNotificationKey]; }
#endif
BOOL ret = NO; IMP app_delegate_application_did_finish_launching_with_options =
SwizzledMethodCache().GetMethodForObject( self, @selector(application:didFinishLaunchingWithOptions:));
if (app_delegate_application_did_finish_launching_with_options) {
ret = ((util::AppDelegateApplicationDidFinishLaunchingWithOptionsFunc)
app_delegate_application_did_finish_launching_with_options)( self, selector_value, application, launch_options);
} else if ([self methodForSelector:@selector(forwardInvocation:)] !=
[NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
[invocation setSelector:selector_value]; [invocation setTarget:self]; [invocation setArgument:&application atIndex:2];
[invocation setArgument:&launch_options atIndex:3]; [self forwardInvocation:invocation];
[invocation getReturnValue:&ret]; } Class notification_center_class = NSClassFromString(@"UNUserNotificationCenter");
if (notification_center_class && application) { LogInfo("Setting up iOS 10 message delegate.");
id user_notification_center = [notification_center_class currentNotificationCenter];
id existing_delegate = [user_notification_center delegate];
if (existing_delegate != nil && existing_delegate != application) { MutexLock lock(g_delegate_mutex);
g_user_delegate = existing_delegate; }
[user_notification_center setDelegate:(id<UNUserNotificationCenterDelegate>)application]; }
NSDictionary* cached_notification = nil; { MutexLock lock(g_delegate_mutex);
if (g_launch_notification != nil && g_delegate.isListenerSet) { cached_notification = g_launch_notification;
g_launch_notification = nil; } } if (cached_notification != nil) {
NotifyApplicationAndServiceOfMessage(cached_notification, true); } return ret; }
static void AppDelegateApplicationDidReceiveRemoteNotification(id self, SEL selector_value, UIApplication *application,
NSDictionary *user_info) {
#if !defined(NDEBUG)
LogInfo("FCM: Received notification (no handler): %s", user_info ? [user_info description].UTF8String : "null");
#else
LogInfo("FCM: Received notification (no handler)");
#endif
if (MessagingIsInitialized()) { NotifyApplicationAndServiceOfMessage(user_info, false); }
IMP app_delegate_application_did_receive_remote_notification = SwizzledMethodCache().GetMethodForObject(
self, @selector(application:didReceiveRemoteNotification:));
if (app_delegate_application_did_receive_remote_notification) {
((util::AppDelegateApplicationDidReceiveRemoteNotificationFunc)
app_delegate_application_did_receive_remote_notification)(self, selector_value, application, user_info);
} else if ([self methodForSelector:@selector(forwardInvocation:)] !=
[NSObject instanceMethodForSelector:@selector(forwardInvocation:)]) {
NSMethodSignature *signature = [[self class] instanceMethodSignatureForSelector:selector_value];
NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
[invocation setSelector:selector_value]; [invocation setTarget:self]; [invocation setArgument:&application atIndex:2];
[invocation setArgument:&user_info atIndex:3]; [self forwardInvocation:invocation]; } }
static void AppDelegateApplicationDidReceiveRemoteNotificationFetchCompletionHandler(
id self, SEL selector_value, UIApplication *application, NSDictionary *user_info,
util::UIBackgroundFetchResultFunction handler) {
#if !defined(NDEBUG)
LogInfo("FCM: Received notification (using handler): %s", user_info ? [user_info description].UTF8String : "null");
#else
LogInfo("FCM: Received notification (using handler)");
#endif
if (MessagingIsInitialized()) { NotifyApplicationAndServiceOfMessage(user_info, false); }
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
[invocation setSelector:selector_value]; [invocation setTarget:self]; [invocation setArgument:&application atIndex:2];
[invocation setArgument:&user_info atIndex:3]; [invocation setArgument:&handler atIndex:4];
[self forwardInvocation:invocation]; } else { handler(UIBackgroundFetchResultNoData); } }
static void HookAppDelegateMethods(Class clazz) { Class method_encoding_class = [FIRSAMAppDelegate class];
auto& method_cache = SwizzledMethodCache(); method_cache.ReplaceOrAddMethod(
clazz, @selector(application:didFinishLaunchingWithOptions:),
(IMP)AppDelegateApplicationDidFinishLaunchingWithOptions, method_encoding_class); method_cache.ReplaceOrAddMethod(
clazz, @selector(application:didRegisterForRemoteNotificationsWithDeviceToken:),
(IMP)AppDelegateApplicationDidRegisterForRemoteNotificationsWithDeviceToken, method_encoding_class);
method_cache.ReplaceOrAddMethod( clazz, @selector(application:didFailToRegisterForRemoteNotificationsWithError:),
(IMP)AppDelegateApplicationDidFailToRegisterForRemoteNotificationsWithError, method_encoding_class);
method_cache.ReplaceOrAddMethod( clazz, @selector(application:didReceiveRemoteNotification:),
(IMP)AppDelegateApplicationDidReceiveRemoteNotification, method_encoding_class); method_cache.ReplaceMethod(
clazz, @selector(application:didReceiveRemoteNotification:fetchCompletionHandler:),
(IMP)AppDelegateApplicationDidReceiveRemoteNotificationFetchCompletionHandler, method_encoding_class); }
static const char kErrorMessageNoRegistrationToken[] =
"Cannot update subscription when SetTokenRegistrationOnInitEnabled is set to false";
static const char kErrorMessageInvalidTopicName[] = "Topic name must not be null or empty.";
static Future<void> PerformTopicOp(MessagingFn fn, const char* topic, const char* op_name, void (^op)(NSString*, void(^)(NSError*))) {
return RunAsyncOp<void>(fn, [topic, op_name, op](ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle) {
if (!topic || topic[0] == ' ') { CompleteAsync(api, handle, kErrorInvalidTopicName, kErrorMessageInvalidTopicName);
return; } if (!IsTokenRegistrationOnInitEnabled()) {
CompleteAsync(api, handle, kErrorNoRegistrationToken, kErrorMessageNoRegistrationToken); return; }
if ([FIRMessaging respondsToSelector:@selector(normalizeTopic:)] && ![FIRMessaging normalizeTopic:@(topic)]) {
std::string error = std::string("Cannot parse topic name ") + topic + ". Will not " + op_name + ".";
CompleteAsync(api, handle, kErrorInvalidTopicName, error.c_str()); return; } op(@(topic), ^void(NSError *error) {
if (internal::g_future_manager.GetFutureApi(&internal::g_future_manager) != api) return;
CompleteAsync(api, handle, error ? kErrorUnknown : kErrorNone, error ? util::NSStringToString(error.localizedDescription).c_str() : "");
}); }); } Future<void> Subscribe(const char* topic) {
return PerformTopicOp(kMessagingFnSubscribe, topic, "subscribe", ^(NSString* t, void(^c)(NSError*)) { [[FIRMessaging messaging] subscribeToTopic:t completion:c]; });
} Future<void> Unsubscribe(const char* topic) {
return PerformTopicOp(kMessagingFnUnsubscribe, topic, "unsubscribe", ^(NSString* t, void(^c)(NSError*)) { [[FIRMessaging messaging] unsubscribeFromTopic:t completion:c]; });
} Future<void> SubscribeLastResult() { return LastResultFn<void>(kMessagingFnSubscribe); }
void SetDeliveryMetricsExportToBigQuery(bool /*enable*/) {
LogWarning("SetDeliveryMetricsExportToBigQuery is not currently implemented on iOS"); }
bool IsRegistrationOnInitEnabled() { return IsTokenRegistrationOnInitEnabled(); }
void SetRegistrationOnInitEnabled(bool enable) { SetTokenRegistrationOnInitEnabled(enable); }
bool IsTokenRegistrationOnInitEnabled() { return [FIRMessaging messaging].autoInitEnabled; }
void SetTokenRegistrationOnInitEnabled(bool enable) { bool was_enabled = IsTokenRegistrationOnInitEnabled();
[FIRMessaging messaging].autoInitEnabled = enable; if (!was_enabled && IsTokenRegistrationOnInitEnabled()) {
if (![g_delegate processCachedRegistrationToken]) { RetrieveRegistrationToken(); } } } Future<void> Register() {
return RunAsyncOp<void>(kMessagingFnRegister, [](ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle) {
[[FIRMessaging messaging] registerWithCompletion:^(NSError *_Nullable error) {
if (internal::g_future_manager.GetFutureApi(&internal::g_future_manager) != api) return;
CompleteAsync(api, handle, error == nullptr ? kErrorNone : kErrorUnknown, error ? util::NSStringToString(error.localizedDescription).c_str() : "");
}]; }); } Future<void> RegisterLastResult() { return LastResultFn<void>(kMessagingFnRegister); }
Future<void> Unregister() {
return RunAsyncOp<void>(kMessagingFnUnregister, [](ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle) {
[[FIRMessaging messaging] unregisterWithCompletion:^(NSError *_Nullable error) {
if (internal::g_future_manager.GetFutureApi(&internal::g_future_manager) != api) return;
CompleteAsync(api, handle, error == nullptr ? kErrorNone : kErrorUnknown, error ? util::NSStringToString(error.localizedDescription).c_str() : "");
}]; }); } Future<void> UnregisterLastResult() { return LastResultFn<void>(kMessagingFnUnregister); }
Future<std::string> GetToken() {
return RunAsyncOp<std::string>(kMessagingFnGetToken, [](ReferenceCountedFutureImpl* api, SafeFutureHandle<std::string> handle) {
[[FIRMessaging messaging] tokenWithCompletion:^(NSString* token, NSError* error) {
if (internal::g_future_manager.GetFutureApi(&internal::g_future_manager) != api) return;
CompleteAsync(api, handle, error ? kErrorUnknown : kErrorNone, error ? util::NSStringToString(error.localizedDescription).c_str() : "", error ? std::string() : util::NSStringToString(token));
}]; }); } Future<std::string> GetTokenLastResult() { return LastResultFn<std::string>(kMessagingFnGetToken); }
Future<void> DeleteTokenLastResult() { return LastResultFn<void>(kMessagingFnDeleteToken); } }
extern "C" void FirebaseMessagingHookAppDelegate(Class app_delegate) {
::firebase::messaging::HookAppDelegateMethods(app_delegate); } @implementation UIApplication (FIRFCM) + (void)load {
NSLog(@"FCM: Loading UIApplication FIRFCM category"); ::firebase::util::RunOnAppDelegateClasses(^(Class clazz) {
FirebaseMessagingHookAppDelegate(clazz); }); }
#if FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
- (void)userNotificationCenter:(UNUserNotificationCenter *)notificationCenter
willPresentNotification:(UNNotification *)notification withCompletionHandler:
(void (^)(UNNotificationPresentationOptions options))completionHandler {
::firebase::LogInfo("FCM: Received notification through notification center.");
if (::firebase::messaging::MessagingIsInitialized()) {
NSDictionary *userInfo = [[[notification request] content] userInfo];
#if !defined(NDEBUG)
::firebase::LogInfo("FCM: userInfo: %s.", userInfo ? userInfo.description.UTF8String : "null");
#endif
::firebase::messaging::NotifyApplicationAndServiceOfMessage(userInfo, false); }
id<UNUserNotificationCenterDelegate> user_delegate = nil; {
::firebase::MutexLock lock(::firebase::messaging::g_delegate_mutex);
user_delegate = ::firebase::messaging::g_user_delegate; } if (user_delegate &&
[user_delegate respondsToSelector:@selector(userNotificationCenter: willPresentNotification: withCompletionHandler:)]) {
[user_delegate userNotificationCenter:notificationCenter willPresentNotification:notification
withCompletionHandler:completionHandler]; } else if (completionHandler) {
completionHandler(UNNotificationPresentationOptionNone); } }
#endif
#if FIREBASE_PLATFORM_IOS
- (void)userNotificationCenter:(UNUserNotificationCenter *)notificationCenter
didReceiveNotificationResponse:(UNNotificationResponse *)response withCompletionHandler:(void (^)())completionHandler {
if (::firebase::messaging::MessagingIsInitialized()) {
NSDictionary *userInfo = [[[[response notification] request] content] userInfo];
#if !defined(NDEBUG)
::firebase::LogInfo("FCM: userInfo: %s.", userInfo ? userInfo.description.UTF8String : "null");
#endif
::firebase::messaging::NotifyApplicationAndServiceOfMessage(userInfo, true); }
id<UNUserNotificationCenterDelegate> user_delegate = nil; {
std::lock_guard<std::mutex> lock(::firebase::messaging::g_delegate_mutex);
user_delegate = ::firebase::messaging::g_user_delegate; } if (user_delegate &&
[user_delegate respondsToSelector:@selector(userNotificationCenter: didReceiveNotificationResponse:
withCompletionHandler:)]) { [user_delegate userNotificationCenter:notificationCenter
didReceiveNotificationResponse:response withCompletionHandler:completionHandler]; } else if (completionHandler) {
completionHandler(); } }
#endif
@end @implementation FIRCppDelegate
- (void)messaging:(FIRMessaging *)messaging didReceiveRegistrationToken:(NSString *)fcmToken { BOOL listener_set = NO; {
firebase::MutexLock lock(::firebase::messaging::g_delegate_mutex); listener_set = _isListenerSet; if (!listener_set) {
_cachedMessaging = messaging; _cachedFCMToken = fcmToken; } } if (listener_set) {
::firebase::LogInfo("FCM: new registration token received."); if (fcmToken.length) {
::firebase::messaging::NotifyListenerOnTokenReceived(fcmToken.UTF8String); } else {
::firebase::messaging::RetrieveRegistrationToken(); } } else { ::firebase::LogInfo(
"FCM: registration token received, but no listener set yet - cached the token."); } }
- (void)messaging:(FIRMessaging *)messaging didReceiveRegistration:(NSString *)installationId {
BOOL listener_set = NO; { firebase::MutexLock lock(::firebase::messaging::g_delegate_mutex);
listener_set = _isListenerSet; if (!listener_set) { _cachedMessaging = messaging;
_cachedRegistrationId = installationId; } } if (listener_set) {
::firebase::LogInfo("FCM: registration installation ID received."); if (installationId != nil) {
::firebase::messaging::NotifyListenerOnRegistrationReceived(installationId.UTF8String); } } else { ::firebase::LogInfo(
"FCM: registration installation ID received, but no listener set yet - cached the ID."); } }
- (void)messaging:(FIRMessaging *)messaging didUnregister:(NSString *)installationId { (void)messaging; {
firebase::MutexLock lock(::firebase::messaging::g_delegate_mutex); _cachedFCMToken = nil; _cachedRegistrationId = nil;
_cachedMessaging = nil; } ::firebase::LogInfo("FCM: unregistration received."); if (installationId != nil) {
::firebase::messaging::NotifyListenerOnUnregistrationReceived(installationId.UTF8String); } }
- (BOOL)processCachedRegistrationToken { FIRMessaging *msg; NSString *token; NSString *regId; {
firebase::MutexLock lock(::firebase::messaging::g_delegate_mutex); if (!_isListenerSet) { return NO; }
msg = _cachedMessaging; token = _cachedFCMToken; regId = _cachedRegistrationId; _cachedFCMToken = nil;
_cachedRegistrationId = nil; _cachedMessaging = nil; } if (token) {
[self messaging:msg didReceiveRegistrationToken:token]; } if (regId) {
[self messaging:msg didReceiveRegistration:regId]; } return token != nil; } @end
