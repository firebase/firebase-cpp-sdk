/*
 * Copyright 2016 Google LLC
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

#include "app/src/util_ios.h"

#include <assert.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <stdlib.h>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"

using firebase::GetLogLevel;
using firebase::kLogLevelDebug;

// Key used in Info.plist to specify a custom AppDelegate class name.
static NSString *const kFirebaseAppDelegateClassNameKey =
    @"FirebaseAppDelegateClassName";

#define MAX_PENDING_APP_DELEGATE_BLOCKS 8
#define MAX_SEEN_DELEGATE_CLASSES 32

static IMP g_original_setDelegate_imp = NULL;
static void (
    ^g_pending_app_delegate_blocks[MAX_PENDING_APP_DELEGATE_BLOCKS])(Class) = {
    nil};
static int g_pending_block_count = 0;
static Class g_seen_delegate_classes[MAX_SEEN_DELEGATE_CLASSES] = {nil};
static int g_seen_delegate_classes_count = 0;

// Function-local static recursive mutex so it is safely initialized even when
// called from Objective-C +load before C++ translation-unit static constructors.
static std::recursive_mutex &GetAppDelegateMutex() {
  static std::recursive_mutex *mutex = new std::recursive_mutex();
  return *mutex;
}

static void Firebase_setDelegate(id self, SEL _cmd,
                                 id<UIApplicationDelegate> delegate) {
  Class new_class = nil;
  if (delegate) {
    new_class = [delegate class];
    if (GetLogLevel() <= kLogLevelDebug)
      NSLog(@"Firebase: UIApplication setDelegate: called with class %s "
            @"(Swizzled)",
            class_getName(new_class));
  } else {
    if (GetLogLevel() <= kLogLevelDebug)
      NSLog(@"Firebase: UIApplication setDelegate: called with nil delegate "
            @"(Swizzled)");
  }

  if (new_class) {
    std::vector<void (^)(Class)> blocks_to_run;
    {
      std::lock_guard<std::recursive_mutex> lock(GetAppDelegateMutex());
      // Check only for exact class match (fixes BUG-1604: subclasses of an
      // already-seen delegate class must still be processed so their overridden
      // methods are swizzled).
      bool direct_class_already_seen = false;
      for (int i = 0; i < g_seen_delegate_classes_count; i++) {
        if (g_seen_delegate_classes[i] == new_class) {
          direct_class_already_seen = true;
          if (GetLogLevel() <= kLogLevelDebug)
            NSLog(@"Firebase: Delegate class %s already seen directly. "
                  @"Skipping processing.",
                  class_getName(new_class));
          break;
        }
      }

      if (!direct_class_already_seen) {
        if (g_seen_delegate_classes_count < MAX_SEEN_DELEGATE_CLASSES) {
          g_seen_delegate_classes[g_seen_delegate_classes_count] = new_class;
          g_seen_delegate_classes_count++;
          if (GetLogLevel() <= kLogLevelDebug)
            NSLog(@"Firebase: Added new delegate class %s to seen list (total "
                  @"seen: %d).",
                  class_getName(new_class), g_seen_delegate_classes_count);

          if (g_pending_block_count > 0) {
            if (GetLogLevel() <= kLogLevelDebug)
              NSLog(@"Firebase: Executing %d pending block(s) for new delegate "
                    @"class: %s.",
                    g_pending_block_count, class_getName(new_class));
            blocks_to_run.reserve(static_cast<size_t>(g_pending_block_count));
            for (int i = 0; i < g_pending_block_count; i++) {
              if (g_pending_app_delegate_blocks[i]) {
                blocks_to_run.push_back(g_pending_app_delegate_blocks[i]);
              }
            }
          }
        } else {
          NSLog(@"Firebase Error: Exceeded MAX_SEEN_DELEGATE_CLASSES (%d). "
                @"Cannot add new delegate class %s or run pending blocks for "
                @"it.",
                MAX_SEEN_DELEGATE_CLASSES, class_getName(new_class));
        }
      }
    }
    for (const auto &pending_block : blocks_to_run) {
      pending_block(new_class);
    }
  }

  // Call the original setDelegate: implementation
  if (g_original_setDelegate_imp) {
    ((void (*)(id, SEL, id<UIApplicationDelegate>))g_original_setDelegate_imp)(
        self, _cmd, delegate);
  } else {
    NSLog(@"Firebase Error: Original setDelegate: IMP not found, cannot call "
          @"original method.");
  }
}

@implementation FIRSAMAppDelegate
- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  return NO;
}
- (BOOL)application:(UIApplication *)application
              openURL:(NSURL *)url
    sourceApplication:(NSString *)sourceApplication
           annotation:(id)annotation {
  return NO;
}
- (BOOL)application:(UIApplication *)application
            openURL:(NSURL *)url
            options:(NSDictionary *)options {
  return NO;
}
- (void)applicationDidBecomeActive:(UIApplication *)application {
}
- (void)applicationDidEnterBackground:(UIApplication *)application {
}
- (void)application:(UIApplication *)application
    didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken {
}
- (void)application:(UIApplication *)application
    didFailToRegisterForRemoteNotificationsWithError:(NSError *)error {
}
- (void)application:(UIApplication *)application
    didReceiveRemoteNotification:(NSDictionary *)userInfo {
}
- (void)application:(UIApplication *)application
    didReceiveRemoteNotification:(NSDictionary *)userInfo
          fetchCompletionHandler:
              (void (^)(UIBackgroundFetchResult result))handler {
}
#if defined(__IPHONE_12_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_12_0
- (BOOL)application:(UIApplication *)application
    continueUserActivity:(NSUserActivity *)userActivity
      restorationHandler:
          (void (^)(NSArray<id<UIUserActivityRestoring>> *restorationHandler))
              restorationHandler {
  return NO;
}
#else
- (BOOL)application:(UIApplication *)application
    continueUserActivity:(NSUserActivity *)userActivity
      restorationHandler:(void (^)(NSArray *))restorationHandler {
  return NO;
}
#endif  // defined(__IPHONE_12_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
        // __IPHONE_12_0
@end

@implementation UIApplication (FirebaseAppDelegateSwizzling)

+ (void)load {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    std::vector<void (^)(Class)> blocks_to_run;
    Class target_specific_class = nil;
    {
      std::lock_guard<std::recursive_mutex> lock(GetAppDelegateMutex());
      NSString *appDelegateClassName = [[NSBundle mainBundle]
          objectForInfoDictionaryKey:kFirebaseAppDelegateClassNameKey];

      if (appDelegateClassName &&
          [appDelegateClassName isKindOfClass:[NSString class]] &&
          appDelegateClassName.length > 0) {
        Class specificClass = NSClassFromString(appDelegateClassName);
        if (specificClass) {
          if (GetLogLevel() <= kLogLevelDebug)
            NSLog(@"Firebase: Info.plist key '%@' found. Targeting AppDelegate "
                  @"class: %@. Swizzling of [UIApplication setDelegate:] will "
                  @"be skipped.",
                  kFirebaseAppDelegateClassNameKey, appDelegateClassName);

          for (int i = 0; i < g_seen_delegate_classes_count; i++) {
            g_seen_delegate_classes[i] = nil;
          }
          g_seen_delegate_classes[0] = specificClass;
          g_seen_delegate_classes_count = 1;
          target_specific_class = specificClass;
          if (GetLogLevel() <= kLogLevelDebug)
            NSLog(@"Firebase: %@ is now the only delegate class Firebase will "
                  @"initially process.",
                  appDelegateClassName);

          if (g_pending_block_count > 0) {
            if (GetLogLevel() <= kLogLevelDebug)
              NSLog(@"Firebase: +load (Info.plist Mode) - Executing %d PENDING "
                    @"block(s) for specified delegate: %@. (Blocks are not "
                    @"removed from queue).",
                    g_pending_block_count, NSStringFromClass(specificClass));
            blocks_to_run.reserve(static_cast<size_t>(g_pending_block_count));
            for (int i = 0; i < g_pending_block_count; i++) {
              if (g_pending_app_delegate_blocks[i]) {
                blocks_to_run.push_back(g_pending_app_delegate_blocks[i]);
              }
            }
          }
        } else {
          NSLog(@"Firebase Error: Info.plist key '%@' specifies class '%@', "
                @"which was not found. Proceeding with default swizzling.",
                kFirebaseAppDelegateClassNameKey, appDelegateClassName);
        }
      } else if (appDelegateClassName) {
        NSLog(@"Firebase Error: Info.plist key '%@' has an invalid value "
              @"('%@'). Proceeding with default swizzling.",
              kFirebaseAppDelegateClassNameKey, appDelegateClassName);
      }

      if (!target_specific_class) {
        // Standard behavior: Swizzle [UIApplication setDelegate:]
        if (GetLogLevel() <= kLogLevelDebug)
          NSLog(@"Firebase: Proceeding with swizzling of [UIApplication "
                @"setDelegate:].");
        Class uiApplicationClass = [UIApplication class];
        SEL originalSelector = @selector(setDelegate:);
        Method originalMethod =
            class_getInstanceMethod(uiApplicationClass, originalSelector);

        if (!originalMethod) {
          NSLog(@"Firebase Error: Original [UIApplication setDelegate:] method "
                @"not found for swizzling.");
          return;
        }

        IMP previousImp =
            method_setImplementation(originalMethod, (IMP)Firebase_setDelegate);
        if (previousImp) {
          g_original_setDelegate_imp = previousImp;
          if (GetLogLevel() <= kLogLevelDebug)
            NSLog(@"Firebase: Successfully swizzled [UIApplication "
                  @"setDelegate:] and stored original IMP.");
        } else {
          NSLog(@"Firebase Error: Swizzled [UIApplication setDelegate:], but "
                @"original IMP was NULL (or method_setImplementation failed to "
                @"return the previous IMP).");
        }
      }
    }

    if (target_specific_class && !blocks_to_run.empty()) {
      for (const auto &pending_block : blocks_to_run) {
        pending_block(target_specific_class);
      }
      if (GetLogLevel() <= kLogLevelDebug)
        NSLog(@"Firebase: +load (Info.plist Mode) - Pending blocks executed "
              @"for specific delegate.");
    }
  });
}

@end

namespace firebase {
namespace util {

void RunOnAppDelegateClasses(void (^block)(Class)) {
  if (!block) return;
  std::vector<Class> seen_classes;
  {
    std::lock_guard<std::recursive_mutex> lock(GetAppDelegateMutex());
    if (g_seen_delegate_classes_count > 0) {
      if (GetLogLevel() <= kLogLevelDebug)
        NSLog(@"Firebase: RunOnAppDelegateClasses executing block for %d "
              @"already seen delegate class(es).",
              g_seen_delegate_classes_count);
      seen_classes.reserve(static_cast<size_t>(g_seen_delegate_classes_count));
      for (int i = 0; i < g_seen_delegate_classes_count; i++) {
        if (g_seen_delegate_classes[i]) {
          seen_classes.push_back(g_seen_delegate_classes[i]);
        }
      }
    } else {
      if (GetLogLevel() <= kLogLevelDebug)
        NSLog(@"Firebase: RunOnAppDelegateClasses - no delegate classes seen "
              @"yet. Block will be queued for future delegates.");
    }

    // Always try to queue the block for any future new delegate classes.
    if (g_pending_block_count < MAX_PENDING_APP_DELEGATE_BLOCKS) {
      g_pending_app_delegate_blocks[g_pending_block_count] = [block copy];
      g_pending_block_count++;
      if (GetLogLevel() <= kLogLevelDebug)
        NSLog(@"Firebase: RunOnAppDelegateClasses - added block to pending "
              @"list (total pending: %d). This block will run on future new "
              @"delegate classes.",
              g_pending_block_count);
    } else {
      NSLog(@"Firebase Error: RunOnAppDelegateClasses - pending block queue is "
            @"full (max %d). Cannot add new block for future execution. "
            @"Discarding block.",
            MAX_PENDING_APP_DELEGATE_BLOCKS);
    }
  }

  for (Class seen_class : seen_classes) {
    block(seen_class);
  }
}

NSDictionary *StringMapToNSDictionary(
    const std::map<std::string, std::string> &string_map) {
  NSMutableDictionary *dictionary =
      [[NSMutableDictionary alloc] initWithCapacity:string_map.size()];
  for (const auto &kv : string_map) {
    NSString *key = StringToNSString(kv.first);
    NSString *val = StringToNSString(kv.second);
    if (key && val) {
      dictionary[key] = val;
    }
  }
  return dictionary;
}

NSDictionary *CharArrayMapToNSDictionary(
    const std::map<const char *, const char *> &string_map) {
  NSMutableDictionary *dictionary =
      [[NSMutableDictionary alloc] initWithCapacity:string_map.size()];
  for (const auto &kv : string_map) {
    if (!kv.first) continue;
    NSString *key = CStringToNSString(kv.first);
    NSString *val = CStringToNSString(kv.second);
    if (key && val) {
      dictionary[key] = val;
    }
  }
  return dictionary;
}

NSData *StringToNSData(const std::string &str) {
  return BytesToNSData(static_cast<const void *>(str.data()), str.length());
}

NSData *BytesToNSData(const char *bytes, const int len) {
  if (!bytes || len <= 0) {
    return [NSData data];
  }
  return [NSData dataWithBytes:bytes length:static_cast<NSUInteger>(len)];
}

NSData *BytesToNSData(const unsigned char *bytes, const int len) {
  if (!bytes || len <= 0) {
    return [NSData data];
  }
  return [NSData dataWithBytes:bytes length:static_cast<NSUInteger>(len)];
}

NSData *BytesToNSData(const void *bytes, size_t len) {
  if (!bytes || len == 0) {
    return [NSData data];
  }
  return [NSData dataWithBytes:bytes length:static_cast<NSUInteger>(len)];
}

std::string NSDataToString(NSData *data) {
  if (!data || !data.bytes || data.length == 0) {
    return std::string();
  }
  return std::string(static_cast<const char *>(data.bytes), data.length);
}

NSString *StringToNSString(const std::string &str) {
  if (str.empty()) {
    return @"";
  }
  NSString *ns_str = [[NSString alloc] initWithBytes:str.data()
                                              length:str.size()
                                            encoding:NSUTF8StringEncoding];
  if (ns_str) {
    return ns_str;
  }
  // Fallback for non-UTF8 byte sequences while preserving embedded NUL bytes.
  ns_str = [[NSString alloc] initWithBytes:str.data()
                                    length:str.size()
                                  encoding:NSISOLatin1StringEncoding];
  return ns_str ? ns_str : @"";
}

NSString *CStringToNSString(const char *c_str) {
  if (!c_str) {
    return @"";
  }
  NSString *ns_str = [NSString stringWithCString:c_str
                                        encoding:NSUTF8StringEncoding];
  if (ns_str) {
    return ns_str;
  }
  // Fallback for non-UTF8 byte sequences so callers never receive nil.
  ns_str = [NSString stringWithCString:c_str
                              encoding:NSISOLatin1StringEncoding];
  return ns_str ? ns_str : @"";
}

std::string NSStringToString(NSString *str) { return StringFromNSString(str); }

NSMutableArray *StringVectorToNSMutableArray(
    const std::vector<std::string> &vector) {
  NSMutableArray<NSString *> *array =
      [[NSMutableArray alloc] initWithCapacity:vector.size()];
  for (const auto &element : vector) {
    NSString *ns_str = StringToNSString(element);
    [array addObject:(ns_str ? ns_str : @"")];
  }
  return array;
}

NSMutableArray *StringUnorderedSetToNSMutableArray(
    const std::unordered_set<std::string> &set) {
  NSMutableArray<NSString *> *array =
      [[NSMutableArray alloc] initWithCapacity:set.size()];
  for (const auto &element : set) {
    NSString *ns_str = StringToNSString(element);
    [array addObject:(ns_str ? ns_str : @"")];
  }
  return array;
}

void NSArrayOfNSStringToVectorOfString(
    NSArray *array, std::vector<std::string> *string_vector) {
  if (!string_vector) return;
  if (!array) return;
  string_vector->reserve(string_vector->size() + array.count);
  for (id object in array) {
    if (![object isKindOfClass:[NSString class]]) {
      FIREBASE_ASSERT_MESSAGE(false, "Object in Array is not of type NSString");
    } else {
      string_vector->push_back(NSStringToString((NSString *)object));
    }
  }
}

NSMutableArray *StdVectorToNSMutableArray(const std::vector<Variant> &vector) {
  NSMutableArray *array =
      [[NSMutableArray alloc] initWithCapacity:vector.size()];
  for (const auto &variant : vector) {
    id obj = VariantToId(variant);
    [array addObject:(obj ? obj : [NSNull null])];
  }
  return array;
}

NSMutableDictionary *StdMapToNSMutableDictionary(
    const std::map<Variant, Variant> &map) {
  NSMutableDictionary *dictionary =
      [[NSMutableDictionary alloc] initWithCapacity:map.size()];
  for (const auto &pair : map) {
    id key = VariantToId(pair.first);
    id val = VariantToId(pair.second);
    if (!key) continue;
    if (!val) val = [NSNull null];
    if ([key conformsToProtocol:@protocol(NSCopying)]) {
      [dictionary setObject:val forKey:key];
    }
  }
  return dictionary;
}

void NSArrayToStdVector(NSArray *array, std::vector<Variant> *vector) {
  if (!vector || !array) return;
  vector->reserve(vector->size() + array.count);
  for (id object in array) {
    vector->push_back(IdToVariant(object));
  }
}

void NSDictionaryToStdMap(NSDictionary *dictionary,
                          std::map<Variant, Variant> *map) {
  if (!map || !dictionary) return;
  for (id key in dictionary) {
    id value = [dictionary objectForKey:key];
    (*map)[IdToVariant(key)] = IdToVariant(value);
  }
}

id VariantToId(const Variant &variant) {
  switch (variant.type()) {
    case Variant::kTypeNull: {
      return [NSNull null];
    }
    case Variant::kTypeInt64: {
      return [NSNumber numberWithLongLong:variant.int64_value()];
    }
    case Variant::kTypeDouble: {
      return [NSNumber numberWithDouble:variant.double_value()];
    }
    case Variant::kTypeBool: {
      return [NSNumber numberWithBool:(variant.bool_value() ? YES : NO)];
    }
    case Variant::kTypeStaticString: {
      return CStringToNSString(variant.string_value());
    }
    case Variant::kTypeMutableString: {
      return StringToNSString(variant.mutable_string());
    }
    case Variant::kTypeVector: {
      return StdVectorToNSMutableArray(variant.vector());
    }
    case Variant::kTypeMap: {
      return StdMapToNSMutableDictionary(variant.map());
    }
    case Variant::kTypeStaticBlob:
    case Variant::kTypeMutableBlob: {
      return BytesToNSData(static_cast<const void *>(variant.blob_data()),
                           variant.blob_size());
    }
    default: {
      FIREBASE_ASSERT_MESSAGE(false, "Variant has invalid type");
      return [NSNull null];
    }
  }
}

static bool IdIsBoolean(id value) {
  if ([value isKindOfClass:[NSNumber class]]) {
    return CFGetTypeID((__bridge CFTypeRef)value) == CFBooleanGetTypeID();
  }
  return false;
}

static bool IdIsInteger(id value) {
  if ([value isKindOfClass:[NSNumber class]]) {
    const char *type = [value objCType];
    if (!type) return false;
    return strcmp(type, @encode(int)) == 0 ||
           strcmp(type, @encode(short)) == 0 ||
           strcmp(type, @encode(long)) == 0 ||
           strcmp(type, @encode(long long)) == 0 ||
           strcmp(type, @encode(char)) == 0 ||
           strcmp(type, @encode(signed char)) == 0 ||
           strcmp(type, @encode(unsigned char)) == 0 ||
           strcmp(type, @encode(unsigned int)) == 0 ||
           strcmp(type, @encode(unsigned short)) == 0 ||
           strcmp(type, @encode(unsigned long)) == 0 ||
           strcmp(type, @encode(unsigned long long)) == 0;
  }
  return false;
}

static bool IdIsFloatingPoint(id value) {
  if ([value isKindOfClass:[NSNumber class]]) {
    const char *type = [value objCType];
    return type && (strcmp(type, @encode(float)) == 0 ||
                    strcmp(type, @encode(double)) == 0);
  }
  return false;
}

Variant IdToVariant(id value) {
  if (value == nil || value == [NSNull null]) {
    return Variant();
  } else if (IdIsBoolean(value)) {
    // Check CFBooleanGetTypeID() before IdIsInteger so kCFBooleanTrue and
    // kCFBooleanFalse are preserved as bool while char/int8_t NSNumbers fall
    // through to IdIsInteger (fixes BUG-0946).
    NSNumber *number = (NSNumber *)value;
    return Variant(number.boolValue != NO ? true : false);
  } else if (IdIsInteger(value)) {
    NSNumber *number = (NSNumber *)value;
    return Variant(number.longLongValue);
  } else if (IdIsFloatingPoint(value)) {
    NSNumber *number = (NSNumber *)value;
    return Variant(number.doubleValue);
  } else if ([value isKindOfClass:[NSString class]]) {
    return Variant::FromMutableString(NSStringToString((NSString *)value));
  } else if ([value isKindOfClass:[NSData class]]) {
    NSData *data = (NSData *)value;
    return Variant::FromMutableBlob(
        (data.length > 0 && data.bytes) ? data.bytes : nullptr, data.length);
  } else if ([value isKindOfClass:[NSURL class]]) {
    return Variant::FromMutableString(StringFromNSUrl((NSURL *)value));
  } else if ([value isKindOfClass:[NSArray class]]) {
    Variant variant = Variant::EmptyVector();
    NSArrayToStdVector((NSArray *)value, &variant.vector());
    return variant;
  } else if ([value isKindOfClass:[NSDictionary class]]) {
    Variant variant = Variant::EmptyMap();
    NSDictionaryToStdMap((NSDictionary *)value, &variant.map());
    return variant;
  } else if ([value isKindOfClass:[NSDate class]]) {
    // Convert NSDates to millis since epoch.
    NSDate *date = (NSDate *)value;
    NSTimeInterval seconds = date.timeIntervalSince1970;
    int64_t millis = static_cast<int64_t>(seconds * 1000);
    return Variant(millis);
  } else {
    NSString *className = NSStringFromClass([value class]);
    LogWarning("Unsupported NSObject type %s.",
               className ? className.UTF8String : "<unknown>");
    FIREBASE_ASSERT_MESSAGE(false, "id has invalid type.");
    return Variant();
  }
}

void DispatchAsyncSafeMainQueue(void (^block)(void)) {
  if (!block) return;
  if ([NSThread isMainThread]) {
    @autoreleasepool {
      block();
    }
  } else {
    dispatch_async(dispatch_get_main_queue(), ^{
      @autoreleasepool {
        block();
      }
    });
  }
}

void RunOnMainThread(void (*function_ptr)(void *function_data),
                     void *function_data) {
  if (!function_ptr) return;
  dispatch_async(dispatch_get_main_queue(), ^{
    @autoreleasepool {
      function_ptr(function_data);
    }
  });
}

void RunOnBackgroundThread(void (*function_ptr)(void *function_data),
                           void *function_data) {
  if (!function_ptr) return;
  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @autoreleasepool {
          function_ptr(function_data);
        }
      });
}

const int ClassMethodImplementationCache::kRandomNameGenerationRetries = 1000;

ClassMethodImplementationCache::ClassMethodImplementationCache() {
  selector_implementation_names_per_selector_ =
      [[NSMutableDictionary alloc] init];
}

ClassMethodImplementationCache::~ClassMethodImplementationCache() {
  selector_implementation_names_per_selector_ = nil;
}

// Replaces an existing method implementation on a class, caching the original
// implementation or adding the method to the class if it doesn't exist.
void ClassMethodImplementationCache::ReplaceOrAddMethod(
    Class clazz, SEL name, IMP imp, Class type_encoding_class,
    bool add_method) {
  if (!clazz || !name || !imp) return;
  std::lock_guard<std::mutex> lock(mutex_);

  const char *class_name = class_getName(clazz);
  Method method = class_getInstanceMethod(clazz, name);
  Class super_class = class_getSuperclass(clazz);
  Method super_method =
      super_class ? class_getInstanceMethod(super_class, name) : nullptr;
  // True only if `clazz` defines its own implementation of `name` rather than
  // inheriting `super_method` from an ancestor class.
  const bool implemented_directly =
      (method != nullptr && method != super_method);

  NSString *selector_name_nsstring = NSStringFromSelector(name);
  const char *selector_name =
      selector_name_nsstring ? selector_name_nsstring.UTF8String : "<null>";

  IMP current_actual_imp = method ? method_getImplementation(method) : nil;

  // === Begin idempotency check ===
  if (current_actual_imp == imp) {
    if (GetLogLevel() <= kLogLevelDebug)
      NSLog(@"Firebase: Method %s on class %s is already swizzled with the "
            @"target IMP. Skipping re-swizzle.",
            selector_name, class_name);
    return;
  }
  // === End idempotency check ===

  IMP original_method_implementation = current_actual_imp;

  // Prefer the existing method's own type encoding when replacing, falling back
  // to type_encoding_class when adding a new method (fixes BUG-2080).
  const char *type_encoding = method ? method_getTypeEncoding(method) : nullptr;
  if (!type_encoding && type_encoding_class) {
    Method encoding_method =
        class_getInstanceMethod(type_encoding_class, name);
    if (encoding_method) {
      type_encoding = method_getTypeEncoding(encoding_method);
    }
  }
  assert(type_encoding);
  if (!type_encoding) {
    NSLog(@"Firebase Error: Type encoding not found for selector %s on class "
          @"%s.",
          selector_name, class_name);
    return;
  }

  NSString *new_method_name_nsstring = nil;
  if (GetLogLevel() <= kLogLevelDebug)
    NSLog(@"Firebase: Attempting to register method for %s selector %s",
          class_name, selector_name);

  if (original_method_implementation) {
    // Try adding a method with randomized prefix on the name.
    bool added = false;
    for (int retry = 0; retry < kRandomNameGenerationRetries; ++retry) {
      new_method_name_nsstring = GenerateRandomSelectorName(name);
      if (!new_method_name_nsstring) continue;
      SEL new_selector = NSSelectorFromString(new_method_name_nsstring);
      if (class_addMethod(clazz, new_selector, original_method_implementation,
                          type_encoding)) {
        added = true;
        break;
      }
    }
    const char *new_method_name =
        new_method_name_nsstring ? new_method_name_nsstring.UTF8String : "<nil>";
    if (!added) {
      NSLog(@"Firebase Error: Failed to add method %s on class %s as the %s "
            @"method already exists on the class. To resolve this issue, "
            @"change the name of the method %s on the class %s.",
            new_method_name, class_name, new_method_name, new_method_name,
            class_name);
      return;
    }
    if (implemented_directly) {
      method_setImplementation(method, imp);
    } else {
      // `clazz` inherited `method` from a superclass; install `imp` on `clazz`
      // without mutating the superclass's Method struct in-place.
      class_replaceMethod(clazz, name, imp, type_encoding);
    }
    // Save the selector name that points at the original method implementation
    // keyed by class so subclass/superclass lookups are deterministic.
    SetMethod(name, clazz, new_method_name_nsstring);
    if (GetLogLevel() <= kLogLevelDebug)
      NSLog(@"Firebase: Registered method for %s selector %s (original method "
            @"%s %p)",
            class_name, selector_name, new_method_name,
            reinterpret_cast<void *>(original_method_implementation));

  } else if (add_method) {
    if (GetLogLevel() <= kLogLevelDebug)
      NSLog(@"Firebase: Adding method for %s selector %s", class_name,
            selector_name);

    // The class doesn't implement the selector so simply install our method
    // implementation.
    if (!class_addMethod(clazz, name, imp, type_encoding)) {
      NSLog(@"Firebase Error: Failed to add new method %s on class %s.",
            selector_name, class_name);
    }
  } else {
    if (GetLogLevel() <= kLogLevelDebug)
      NSLog(@"Firebase: Method implementation for %s selector %s not found, "
            @"ignoring.",
            class_name, selector_name);
  }
}

// Get the original method implementation for the specific selector.
IMP ClassMethodImplementationCache::GetMethod(Class clazz, SEL name) {
  if (!clazz || !name) return nil;
  std::lock_guard<std::mutex> lock(mutex_);
  assert(selector_implementation_names_per_selector_);

  NSString *selector_name_nsstring = NSStringFromSelector(name);
  if (!selector_name_nsstring) return nil;
  const char *selector_name = selector_name_nsstring.UTF8String;
  NSDictionary *selector_implementation_names =
      selector_implementation_names_per_selector_[selector_name_nsstring];
  const char *class_name = class_getName(clazz);
  if (!selector_implementation_names) {
    if (GetLogLevel() <= kLogLevelDebug)
      NSLog(@"Firebase: Method not cached for class %s selector %s.",
            class_name, selector_name);
    return nil;
  }

  struct HierarchyMatch {
    Class matched_class;
    NSString *impl_selector_name;
    IMP imp;
  };
  std::vector<HierarchyMatch> matches;

  // Walk the class hierarchy from `clazz` upward so a subclass's cached
  // original implementation is always found before a superclass's cached
  // implementation regardless of NSDictionary hash order (fixes BUG-1526).
  for (Class search_class = clazz; search_class;
       search_class = class_getSuperclass(search_class)) {
    NSString *search_class_key = NSStringFromClass(search_class);
    NSString *direct_impl_name =
        search_class_key ? selector_implementation_names[search_class_key]
                         : nil;
    if (direct_impl_name) {
      SEL selector_implementation = NSSelectorFromString(direct_impl_name);
      Method method =
          class_getInstanceMethod(search_class, selector_implementation);
      IMP candidate_imp = method ? method_getImplementation(method) : nil;
      if (candidate_imp) {
        matches.push_back({search_class, direct_impl_name, candidate_imp});
      }
    }
  }

  if (matches.empty()) {
    if (GetLogLevel() <= kLogLevelDebug)
      NSLog(@"Firebase: Class %s does not respond to selector %s", class_name,
            selector_name);
    return nil;
  }

  size_t selected_index = 0;
  if (matches.size() > 1) {
    // Both a subclass and one or more superclasses in `clazz`'s hierarchy were
    // swizzled for `name`. If a subclass's original implementation called
    // `[super name]`, that subclass's original IMP is currently on the call
    // stack; advance past it to resolve the superclass's original IMP instead
    // of recursing into the subclass's IMP again.
    void *callstack[64];
    const int frame_count = backtrace(callstack, 64);
    for (size_t idx = 0; idx + 1 < matches.size(); ++idx) {
      const uintptr_t target_imp =
          reinterpret_cast<uintptr_t>(matches[idx].imp);
      bool on_stack = false;
      for (int f = 1; f < frame_count; ++f) {
        Dl_info info;
        if (dladdr(callstack[f], &info) != 0 && info.dli_saddr != nullptr) {
          if (reinterpret_cast<uintptr_t>(info.dli_saddr) == target_imp) {
            on_stack = true;
            break;
          }
        } else {
          const uintptr_t pc = reinterpret_cast<uintptr_t>(callstack[f]);
          if (pc >= target_imp && (pc - target_imp) < 4096) {
            on_stack = true;
            break;
          }
        }
      }
      if (on_stack) {
        selected_index = idx + 1;
      }
    }
  }

  const HierarchyMatch &chosen = matches[selected_index];
  if (GetLogLevel() <= kLogLevelDebug)
    NSLog(@"Firebase: Found %s (%s, %p) on class %s (%s)", selector_name,
          chosen.impl_selector_name ? chosen.impl_selector_name.UTF8String
                                    : "<unknown>",
          reinterpret_cast<void *>(chosen.imp), class_name,
          class_getName(chosen.matched_class));

  return chosen.imp;
}

ClassMethodImplementationCache *ClassMethodImplementationCache::GetCreateCache(
    ClassMethodImplementationCache **cache) {
  assert(cache);
  if (!cache) return nullptr;
  static std::mutex *create_mutex = new std::mutex();
  std::lock_guard<std::mutex> lock(*create_mutex);
  if (!(*cache)) *cache = new ClassMethodImplementationCache();
  return *cache;
}

void ClassMethodImplementationCache::SetMethod(
    SEL name, Class clazz, NSString *implementation_selector_name) {
  NSString *selector_name = NSStringFromSelector(name);
  NSString *class_key = clazz ? NSStringFromClass(clazz) : nil;
  if (!selector_name || !class_key || !implementation_selector_name) return;
  NSMutableDictionary *implementation_selector_names =
      selector_implementation_names_per_selector_[selector_name];
  if (!implementation_selector_names) {
    implementation_selector_names = [[NSMutableDictionary alloc] init];
    selector_implementation_names_per_selector_[selector_name] =
        implementation_selector_names;
  }
  implementation_selector_names[class_key] = implementation_selector_name;
}

// Generate a random method name from the specified selector name. This is
// used to store the implementation of an overridden method.
NSString *ClassMethodImplementationCache::GenerateRandomSelectorName(SEL name) {
  NSString *selector_name = NSStringFromSelector(name);
  if (!selector_name) return nil;
  NSDictionary *implementation_selector_names =
      selector_implementation_names_per_selector_[selector_name];
  NSArray *existing_names =
      implementation_selector_names ? [implementation_selector_names allValues]
                                    : nil;
  for (int retry = 0; retry < kRandomNameGenerationRetries; ++retry) {
    NSString *random_selector_name = [[NSString alloc]
        initWithFormat:@"FIRA%x%@", arc4random(), selector_name];
    if (!existing_names ||
        ![existing_names containsObject:random_selector_name]) {
      return random_selector_name;
    }
  }
  return nil;
}

}  // namespace util
}  // namespace firebase
