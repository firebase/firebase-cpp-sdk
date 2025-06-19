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

#include "app/src/assert.h"

#include <assert.h>
#include <stdlib.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#define MAX_PENDING_APP_DELEGATE_BLOCKS 8
#define MAX_SEEN_DELEGATE_CLASSES 32

static IMP g_original_setDelegate_imp = NULL;
static void (^g_pending_app_delegate_blocks[MAX_PENDING_APP_DELEGATE_BLOCKS])(Class) = {nil};
static int g_pending_block_count = 0;

static Class g_seen_delegate_classes[MAX_SEEN_DELEGATE_CLASSES] = {nil};
static int g_seen_delegate_classes_count = 0;

// Swizzled implementation of setDelegate:
static void Firebase_setDelegate(id self, SEL _cmd, id<UIApplicationDelegate> delegate) {
  Class new_class = nil;
  if (delegate) {
    new_class = [delegate class];
    NSLog(@"Firebase: UIApplication setDelegate: called with class %s (Swizzled)",
          class_getName(new_class));
  } else {
    NSLog(@"Firebase: UIApplication setDelegate: called with nil delegate (Swizzled)");
    // If delegate is nil, new_class remains nil.
    // The original implementation will be called later.
    // No class processing or block execution needed.
  }

  if (new_class) {
    // 1. Superclass Check
    bool superclass_already_seen = false;
    Class current_super = class_getSuperclass(new_class);
    while (current_super) {
      for (int i = 0; i < g_seen_delegate_classes_count; i++) {
        if (g_seen_delegate_classes[i] == current_super) {
          superclass_already_seen = true;
          NSLog(@"Firebase: Delegate class %s has superclass %s which was already seen. Skipping processing for %s.",
                class_getName(new_class), class_getName(current_super), class_getName(new_class));
          break;
        }
      }
      if (superclass_already_seen) break;
      current_super = class_getSuperclass(current_super);
    }

    if (!superclass_already_seen) {
      // 2. Direct Class Check (if no superclass was seen)
      bool direct_class_already_seen = false;
      for (int i = 0; i < g_seen_delegate_classes_count; i++) {
        if (g_seen_delegate_classes[i] == new_class) {
          direct_class_already_seen = true;
          NSLog(@"Firebase: Delegate class %s already seen directly. Skipping processing.",
                class_getName(new_class));
          break;
        }
      }

      if (!direct_class_already_seen) {
        // 3. Process as New Class
        if (g_seen_delegate_classes_count < MAX_SEEN_DELEGATE_CLASSES) {
          g_seen_delegate_classes[g_seen_delegate_classes_count] = new_class;
          g_seen_delegate_classes_count++;
          NSLog(@"Firebase: Added new delegate class %s to seen list (total seen: %d).",
                class_getName(new_class), g_seen_delegate_classes_count);

          if (g_pending_block_count > 0) {
            NSLog(@"Firebase: Executing %d pending block(s) for new delegate class: %s.",
                  g_pending_block_count, class_getName(new_class));
            for (int i = 0; i < g_pending_block_count; i++) {
              if (g_pending_app_delegate_blocks[i]) {
                g_pending_app_delegate_blocks[i](new_class);
              }
            }
          }
        } else {
          NSLog(@"Firebase Error: Exceeded MAX_SEEN_DELEGATE_CLASSES (%d). Cannot add new delegate class %s or run pending blocks for it.",
                MAX_SEEN_DELEGATE_CLASSES, class_getName(new_class));
        }
      }
    }
  }

  // Call the original setDelegate: implementation
  if (g_original_setDelegate_imp) {
    ((void (*)(id, SEL, id<UIApplicationDelegate>))g_original_setDelegate_imp)(self, _cmd, delegate);
  } else {
    NSLog(@"Firebase Error: Original setDelegate: IMP not found, cannot call original method.");
  }
}

@implementation UIApplication (FirebaseAppDelegateSwizzling)

+ (void)load {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    Class uiApplicationClass = [UIApplication class];
    SEL originalSelector = @selector(setDelegate:);
    Method originalMethod = class_getInstanceMethod(uiApplicationClass, originalSelector);

    if (!originalMethod) {
      NSLog(@"Firebase Error: Original [UIApplication setDelegate:] method not found for swizzling.");
      return;
    }

    IMP previousImp = method_setImplementation(originalMethod, (IMP)Firebase_setDelegate);
    if (previousImp) {
        g_original_setDelegate_imp = previousImp;
        NSLog(@"Firebase: Successfully swizzled [UIApplication setDelegate:] and stored original IMP.");
    } else {
        NSLog(@"Firebase Error: Swizzled [UIApplication setDelegate:], but original IMP was NULL (or method_setImplementation failed).");
    }
  });
}

@end

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
          fetchCompletionHandler:(void (^)(UIBackgroundFetchResult result))handler {
}
#if defined(__IPHONE_12_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_12_0
- (BOOL)application:(UIApplication *)application
    continueUserActivity:(NSUserActivity *)userActivity
      restorationHandler:
          (void (^)(NSArray<id<UIUserActivityRestoring>> *restorationHandler))restorationHandler {
  return NO;
}
#else
- (BOOL)application:(UIApplication *)application
    continueUserActivity:(NSUserActivity *)userActivity
      restorationHandler:(void (^)(NSArray *))restorationHandler {
  return NO;
}
#endif  // defined(__IPHONE_12_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_12_0
@end

namespace firebase {
namespace util {

void RunOnAppDelegateClasses(void (^block)(Class)) {
  if (g_seen_delegate_classes_count > 0) {
    NSLog(@"Firebase: RunOnAppDelegateClasses executing block for %d already seen delegate class(es).",
          g_seen_delegate_classes_count);
    for (int i = 0; i < g_seen_delegate_classes_count; i++) {
      // Assuming classes in g_seen_delegate_classes up to count are non-nil
      if (g_seen_delegate_classes[i]) { // Additional safety check
        block(g_seen_delegate_classes[i]);
      }
    }
  } else {
    NSLog(@"Firebase: RunOnAppDelegateClasses - no delegate classes seen yet. Block will be queued for future delegates.");
  }

  // Always try to queue the block for any future new delegate classes.
  // This block will be executed by Firebase_setDelegate if a new delegate class is set.
  if (g_pending_block_count < MAX_PENDING_APP_DELEGATE_BLOCKS) {
    g_pending_app_delegate_blocks[g_pending_block_count] = [block copy];
    g_pending_block_count++;
    NSLog(@"Firebase: RunOnAppDelegateClasses - added block to pending list (total pending: %d). This block will run on future new delegate classes.", g_pending_block_count);
  } else {
    NSLog(@"Firebase Error: RunOnAppDelegateClasses - pending block queue is full (max %d). Cannot add new block for future execution. Discarding block.", MAX_PENDING_APP_DELEGATE_BLOCKS);
    // Block is discarded for future execution.
  }
}

NSDictionary *StringMapToNSDictionary(const std::map<std::string, std::string> &string_map) {
  NSMutableDictionary *dictionary = [[NSMutableDictionary alloc] init];
  for (auto &kv : string_map) {
    dictionary[[NSString stringWithUTF8String:kv.first.c_str()]] =
        [NSString stringWithUTF8String:kv.second.c_str()];
  }
  return dictionary;
}

NSDictionary *CharArrayMapToNSDictionary(const std::map<const char *, const char *> &string_map) {
  NSMutableDictionary *dictionary = [[NSMutableDictionary alloc] init];
  for (auto &kv : string_map) {
    dictionary[[NSString stringWithUTF8String:kv.first]] =
        [NSString stringWithUTF8String:kv.second];
  }
  return dictionary;
}

NSData *StringToNSData(const std::string &str) {
  return BytesToNSData(str.data(), static_cast<int>(str.length()));
}

NSData *BytesToNSData(const char *bytes, const int len) {
  return [NSData dataWithBytes:bytes length:len];
}

NSData *BytesToNSData(const unsigned char *bytes, const int len) {
  return [NSData dataWithBytes:bytes length:len];
}

std::string NSDataToString(NSData *data) {
  return std::string(static_cast<const char *>(data.bytes), data.length);
}

NSString *StringToNSString(const std::string &str) { return CStringToNSString(str.c_str()); }

NSString *CStringToNSString(const char *c_str) {
  return [NSString stringWithCString:c_str encoding:NSUTF8StringEncoding];
}

std::string NSStringToString(NSString *str) {
  return str ? std::string([str cStringUsingEncoding:NSUTF8StringEncoding]) : std::string();
}

NSMutableArray *StringVectorToNSMutableArray(const std::vector<std::string> &vector) {
  NSMutableArray<NSString *> *array = [[NSMutableArray alloc] initWithCapacity:vector.size()];
  for (auto &element : vector) {
    [array addObject:StringToNSString(element)];
  }
  return array;
}

NSMutableArray *StringUnorderedSetToNSMutableArray(const std::unordered_set<std::string> &set) {
  NSMutableArray<NSString *> *array = [[NSMutableArray alloc] initWithCapacity:set.size()];
  for (auto &element : set) {
    [array addObject:StringToNSString(element)];
  }
  return array;
}

void NSArrayOfNSStringToVectorOfString(NSArray *array, std::vector<std::string> *string_vector) {
  string_vector->reserve(array.count);
  for (id object in array) {
    if (![object isKindOfClass:[NSString class]]) {
      FIREBASE_ASSERT_MESSAGE(false, "Object in Array is not of type NSString");
    } else {
      string_vector->push_back(NSStringToString((NSString *)object));
    }
  }
}

NSMutableArray *StdVectorToNSMutableArray(const std::vector<Variant> &vector) {
  NSMutableArray *array = [[NSMutableArray alloc] initWithCapacity:vector.size()];
  for (auto &variant : vector) {
    [array addObject:VariantToId(variant)];
  }
  return array;
}

NSMutableDictionary *StdMapToNSMutableDictionary(const std::map<Variant, Variant> &map) {
  NSMutableDictionary *dictionary = [[NSMutableDictionary alloc] initWithCapacity:map.size()];
  for (auto &pair : map) {
    [dictionary setObject:VariantToId(pair.second) forKey:VariantToId(pair.first)];
  }
  return dictionary;
}

void NSArrayToStdVector(NSArray *array, std::vector<Variant> *vector) {
  vector->reserve(array.count);
  for (id object in array) {
    vector->push_back(IdToVariant(object));
  }
}

void NSDictionaryToStdMap(NSDictionary *dictionary, std::map<Variant, Variant> *map) {
  for (id key in dictionary) {
    id value = [dictionary objectForKey:key];
    map->insert(std::make_pair(IdToVariant(key), IdToVariant(value)));
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
      return [NSNumber numberWithBool:variant.bool_value()];
    }
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString: {
      return @(variant.string_value());
    }
    case Variant::kTypeVector: {
      return StdVectorToNSMutableArray(variant.vector());
    }
    case Variant::kTypeMap: {
      return StdMapToNSMutableDictionary(variant.map());
    }
    default: {
      FIREBASE_ASSERT_MESSAGE(false, "Variant has invalid type");
      return [NSNull null];
    }
  }
}

static bool IdIsBoolean(id value) {
  if ([value isKindOfClass:[NSNumber class]]) {
    const char *type = [value objCType];
    return strcmp(type, @encode(BOOL)) == 0 || strcmp(type, @encode(signed char)) == 0;
  }
  return false;
}

static bool IdIsInteger(id value) {
  if ([value isKindOfClass:[NSNumber class]]) {
    const char *type = [value objCType];
    return strcmp(type, @encode(int)) == 0 || strcmp(type, @encode(short)) == 0 ||
           strcmp(type, @encode(long)) == 0 || strcmp(type, @encode(long long)) == 0 ||
           strcmp(type, @encode(unsigned char)) == 0 || strcmp(type, @encode(unsigned int)) == 0 ||
           strcmp(type, @encode(unsigned short)) == 0 ||
           strcmp(type, @encode(unsigned long)) == 0 ||
           strcmp(type, @encode(unsigned long long)) == 0;
  }
  return false;
}

static bool IdIsFloatingPoint(id value) {
  if ([value isKindOfClass:[NSNumber class]]) {
    const char *type = [value objCType];
    return strcmp(type, @encode(float)) == 0 || strcmp(type, @encode(double)) == 0;
  }
  return false;
}

Variant IdToVariant(id value) {
  if (value == nil || value == [NSNull null]) {
    return Variant();
  } else if (IdIsInteger(value)) {
    NSNumber *number = (NSNumber *)value;
    return Variant(number.longLongValue);
  } else if (IdIsFloatingPoint(value)) {
    NSNumber *number = (NSNumber *)value;
    return Variant(number.doubleValue);
  } else if (IdIsBoolean(value)) {
    NSNumber *number = (NSNumber *)value;
    return Variant(number.boolValue != NO ? true : false);
  } else if ([value isKindOfClass:[NSString class]]) {
    return Variant::FromMutableString([value UTF8String]);
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
    LogWarning("Unsupported NSObject type %s.", className.UTF8String);
    FIREBASE_ASSERT_MESSAGE(false, "id has invalid type.");
    return Variant();
  }
}

void DispatchAsyncSafeMainQueue(void (^block)(void)) {
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_async(dispatch_get_main_queue(), block);
  }
}

void RunOnMainThread(void (*function_ptr)(void *function_data), void *function_data) {
  dispatch_async(dispatch_get_main_queue(), ^{
    function_ptr(function_data);
  });
}

void RunOnBackgroundThread(void (*function_ptr)(void *function_data), void *function_data) {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    function_ptr(function_data);
  });
}

const int ClassMethodImplementationCache::kRandomNameGenerationRetries = 1000;

ClassMethodImplementationCache::ClassMethodImplementationCache() {
  selector_implementation_names_per_selector_ = [[NSMutableDictionary alloc] init];
}

ClassMethodImplementationCache::~ClassMethodImplementationCache() {
  selector_implementation_names_per_selector_ = nil;
}

// Replaces an existing method implementation on a class, caching the original implementation or
// adding the method to the class if it doesn't exist.
//
// If we need to add the method, we will use the type encoding of the selector |name| on
// |type_encoding_class|.  The |type_encoding_class| is only used if a method isn't found for the
// specified selector |name| and therefore a new method needs to be added to the class.
//
// The cached method method implementation can be retrieved using GetMethod().
void ClassMethodImplementationCache::ReplaceOrAddMethod(Class clazz, SEL name, IMP imp,
                                                        Class type_encoding_class,
                                                        bool add_method) {
  const char *class_name = class_getName(clazz);
  Method method = class_getInstanceMethod(clazz, name);
  NSString *selector_name_nsstring = NSStringFromSelector(name);
  const char *selector_name = selector_name_nsstring.UTF8String;
  IMP original_method_implementation = method ? method_getImplementation(method) : nil; // Directly initialized

  // Get the type encoding of the selector from a type_encoding_class (which is a class which
  // implements a stub for the method).
  const char *type_encoding =
      method_getTypeEncoding(class_getInstanceMethod(type_encoding_class, name));
  assert(type_encoding);

  NSString *new_method_name_nsstring = nil;
  if (GetLogLevel() <= kLogLevelDebug) {
    // This log might have been just "Registering method..." or similar.
    // Let's revert to a more basic version if it was changed, or ensure it's reasonable.
    // For the purpose of this revert, keeping the "Firebase Cache: Attempting to register..."
    // or reverting to a simpler "Registering method..." is acceptable if the exact prior state
    // of this specific log line is not critical, the main point is the logic revert.
    // Let's assume it was:
    NSLog(@"Firebase Cache: Registering method for %s selector %s", class_name, selector_name);
  }
  if (original_method_implementation) {
    // Try adding a method with randomized prefix on the name.
    int retry = kRandomNameGenerationRetries;
    while (retry--) {
      // Cache the old method implementation in a new method so that we can lookup the original
      // implementation from the instance of the class.
      new_method_name_nsstring = GenerateRandomSelectorName(name);
      SEL new_selector = NSSelectorFromString(new_method_name_nsstring);
      if (class_addMethod(clazz, new_selector, original_method_implementation, type_encoding)) {
        break;
      }
    }
    const char *new_method_name = new_method_name_nsstring.UTF8String;
    if (retry == 0) {
      NSLog(@"Failed to add method %s on class %s as the %s method already exists on the class. To "
            @"resolve this issue, change the name of the method %s on the class %s.",
            new_method_name, class_name, new_method_name, new_method_name, class_name);
      return;
    }
    method_setImplementation(method, imp);
    // Save the selector name that points at the original method implementation.
    SetMethod(name, new_method_name_nsstring);
    if (GetLogLevel() <= kLogLevelDebug) {
      NSLog(@"Registered method for %s selector %s (original method %s 0x%08x)", class_name,
            selector_name, new_method_name,
            static_cast<int>(reinterpret_cast<intptr_t>(original_method_implementation)));
    }
  } else if (add_method) {
    if (GetLogLevel() <= kLogLevelDebug) {
      NSLog(@"Adding method for %s selector %s", class_name, selector_name);
    }
    // The class doesn't implement the selector so simply install our method implementation.
    if (!class_addMethod(clazz, name, imp, type_encoding)) {
      NSLog(@"Failed to add new method %s on class %s.", selector_name, class_name);
    }
  } else {
    if (GetLogLevel() <= kLogLevelDebug) {
      NSLog(@"Method implementation for %s selector %s not found, ignoring.", class_name,
            selector_name);
    }
  }
}

// Get the original method implementation for the specific selector.
IMP ClassMethodImplementationCache::GetMethod(Class clazz, SEL name) {
  assert(selector_implementation_names_per_selector_);

  NSString *selector_name_nsstring = NSStringFromSelector(name);
  const char *selector_name = selector_name_nsstring.UTF8String;
  NSDictionary *selector_implementation_names =
      selector_implementation_names_per_selector_[selector_name_nsstring];
  const char *class_name = class_getName(clazz);
  if (!selector_implementation_names) {
    if (GetLogLevel() <= kLogLevelDebug) {
      NSLog(@"Method not cached for class %s selector %s.", class_name, selector_name);
    }
    return nil;
  }

  // Search for one of our randomly generated selector names on this class.
  IMP method_implementation = nil;
  NSString *selector_implementation_name_nsstring = nil;
  Class search_class = nil;
  for (NSString *key in selector_implementation_names) {
    selector_implementation_name_nsstring = key;
    // This object may have been dynamically subclassed like (http://goto/scion-app-delegate-proxy)
    // or replaced entirely so search each superclass to see whether they respond to the
    // implementation selector.
    const char *selector_implementation_name = selector_implementation_name_nsstring.UTF8String;
    SEL selector_implementation = NSSelectorFromString(selector_implementation_name_nsstring);
    search_class = clazz;
    for (; search_class; search_class = class_getSuperclass(search_class)) {
      const char *search_class_name = class_getName(search_class);
      if (GetLogLevel() <= kLogLevelDebug) {
        NSLog(@"Searching for selector %s (%s) on class %s", selector_name,
              selector_implementation_name, search_class_name);
      }
      Method method = class_getInstanceMethod(search_class, selector_implementation);
      method_implementation = method ? method_getImplementation(method) : nil;
      if (method_implementation) break;
    }
    if (method_implementation) break;
  }
  if (!method_implementation) {
    if (GetLogLevel() <= kLogLevelDebug) {
      NSLog(@"Class %s does not respond to selector %s (%s)", class_name, selector_name,
            selector_implementation_name_nsstring.UTF8String);
    }
    return nil;
  }
  if (GetLogLevel() <= kLogLevelDebug) {
    NSLog(@"Found %s (%s, 0x%08x) on class %s (%s)", selector_name,
          selector_implementation_name_nsstring.UTF8String,
          static_cast<int>(reinterpret_cast<intptr_t>(method_implementation)), class_name,
          class_getName(search_class));
  }
  return method_implementation;
}

ClassMethodImplementationCache *ClassMethodImplementationCache::GetCreateCache(
    ClassMethodImplementationCache **cache) {
  assert(cache);
  if (!(*cache)) *cache = new ClassMethodImplementationCache();
  return *cache;
}

void ClassMethodImplementationCache::SetMethod(SEL name, NSString *implementation_selector_name) {
  NSString *selector_name = NSStringFromSelector(name);
  NSMutableDictionary *implementation_selector_names =
      selector_implementation_names_per_selector_[selector_name];
  // Create the dictionary of implementation selector names and add it to the dictionary if it's
  // not already present.
  if (!implementation_selector_names) {
    implementation_selector_names = [[NSMutableDictionary alloc] init];
    selector_implementation_names_per_selector_[selector_name] = implementation_selector_names;
  }
  implementation_selector_names[implementation_selector_name] = implementation_selector_name;
}

// Generate a random method name from the specified selector name.  This is
// used to store the implementation of an overridden method.
NSString *ClassMethodImplementationCache::GenerateRandomSelectorName(SEL name) {
  NSString *selector_name = NSStringFromSelector(name);
  NSDictionary *implementation_selector_names =
      selector_implementation_names_per_selector_[selector_name];
  int retry = kRandomNameGenerationRetries;
  while (retry--) {
    // Cache the old method implementation in a new method so that we can lookup the original
    // implementation from the instance of the class.
    NSString *random_selector_name =
        [[NSString alloc] initWithFormat:@"FIRA%x%@", arc4random(), selector_name];
    if (!implementation_selector_names[random_selector_name]) {
      return random_selector_name;
    }
  }
  return nil;
}

}  // namespace util
}  // namespace firebase
