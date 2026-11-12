// Copyright 2026 Google LLC
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

#ifndef FIREBASE_APP_SRC_APPLE_DESKTOP_RUNTIME_INTERNAL_H_
#define FIREBASE_APP_SRC_APPLE_DESKTOP_RUNTIME_INTERNAL_H_

// Runtime adaptations for running the statically embedded Firebase Apple SDK
// inside macOS desktop processes (standalone CMake executables, unprovisioned
// .app bundles, and the Unity Editor):
//
//  1. Unprovisioned Data Protection Keychain fallback (errSecMissingEntitlement
//     / -34018): GoogleUtilities' GULKeychainUtils sets
//     kSecUseDataProtectionKeychain = YES on macOS 10.15+, which requires an
//     Apple provisioning profile with an application-identifier entitlement.
//     Signed/provisioned macOS apps use the real Data Protection Keychain
//     untouched. Unprovisioned processes (such as Unity.app, ad-hoc CMake test
//     bundles, or local developer builds) fail SecItemAdd with -34018, breaking
//     both FirebaseInstallations and FIRAnalytics' internal FID reporting. When
//     and only when GULKeychainUtils lives in the current Mach-O image, its
//     three Keychain class methods fall back on -34018 to 0600 files inside a
//     0700 user-only directory under ~/Library/Application Support.
//
//  2. Headless / non-CFRunLoop promise dispatch: FBLPromise defaults its
//     dispatch queue to dispatch_get_main_queue(), which stalls forever in C++
//     CLI / integration-test loops and Unity -batchmode processes that wait on
//     firebase::Future without pumping the main CFRunLoop. When FBLPromise
//     lives in the current Mach-O image and still points at the main queue, we
//     move its defaultDispatchQueue to a dedicated serial background queue.
//
//  3. Initial active-app session state (APMScreenViewReporter): on macOS,
//     GoogleAppMeasurement initializes APMScreenViewReporter._isAppActive to NO
//     and relies on NSApplicationDidBecomeActiveNotification firing after
//     +[FIRApp configure] to mark the app active, log session_start (_s), and
//     populate _sid for GetSessionId(). That notification never fires in CLI /
//     headless desktop binaries and has already fired before Play Mode starts
//     in the Unity Editor (causing GetSessionId() to fail with "Session
//     expired"). EnsureAnalyticsAppActiveState() transitions the embedded
//     APMScreenViewReporter singleton to active on the main thread without
//     posting synthetic NSNotifications to the host application.
//
// Like analytics_desktop_apple.mm and installations_desktop_apple.mm, this
// header uses only Foundation, libSystem (CommonCrypto, dlfcn, POSIX), and the
// Objective-C runtime, keeping zero link-time dependency on the Apple SDK or
// Security.framework.

#import <CommonCrypto/CommonDigest.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import <dlfcn.h>
#import <objc/message.h>
#import <objc/runtime.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "app/src/log.h"

#if !__has_feature(objc_arc)
#error "apple_desktop_runtime_internal.h must be compiled with -fobjc-arc"
#endif

@protocol FIRCppAppleDesktopPromiseClassShape <NSObject>
+ (dispatch_queue_t)defaultDispatchQueue;
+ (void)setDefaultDispatchQueue:(dispatch_queue_t)queue;
@end

@protocol FIRCppAppleDesktopScreenViewReporterShape <NSObject>
- (BOOL)isAppActive;
- (void)handleApplicationDidBecomeActive:(nullable NSNotification*)notification;
@end

@protocol FIRCppAppleDesktopScreenViewReporterClassShape <NSObject>
+ (nullable id<FIRCppAppleDesktopScreenViewReporterShape>)sharedInstance;
@end

namespace firebase {
namespace internal {
namespace apple_desktop {
namespace {

using GetItemImp = NSData* (*)(id, SEL, NSDictionary*, NSError* __autoreleasing*);
using SetItemImp = BOOL (*)(id, SEL, NSData*, NSDictionary*, NSError* __autoreleasing*);
using RemoveItemImp = BOOL (*)(id, SEL, NSDictionary*, NSError* __autoreleasing*);

constexpr NSInteger kSecMissingEntitlementStatus = -34018;

GetItemImp g_orig_get_item = nullptr;
SetItemImp g_orig_set_item = nullptr;
RemoveItemImp g_orig_remove_item = nullptr;
bool g_logged_keychain_fallback = false;
dispatch_queue_t g_fbl_promise_queue = nil;

// True while executing g_orig_set_item on the current thread so its internal
// probe `[self getItemWithQuery:...]` queries only the real Keychain instead of
// seeing a fallback file and routing into SecItemUpdate (-25300).
thread_local bool g_in_original_set_item = false;

class ScopedOriginalSetItemFlag {
 public:
  ScopedOriginalSetItemFlag() : prev_(g_in_original_set_item) { g_in_original_set_item = true; }
  ~ScopedOriginalSetItemFlag() { g_in_original_set_item = prev_; }

 private:
  bool prev_;
};

__attribute__((noinline)) const void* CurrentImageAnchor() {
  return reinterpret_cast<const void*>(&CurrentImageAnchor);
}

// Returns true iff `cls` resides in the exact same Mach-O image (executable or
// plugin bundle) as this translation unit, ensuring we never alter classes
// belonging to a host application's own frameworks.
bool IsClassInCurrentImage(Class cls) {
  if (cls == Nil) return false;
  Dl_info self_info{};
  Dl_info cls_info{};
  if (dladdr(CurrentImageAnchor(), &self_info) == 0 ||
      dladdr((__bridge const void*)cls, &cls_info) == 0) {
    return false;
  }
  return self_info.dli_fbase != nullptr && self_info.dli_fbase == cls_info.dli_fbase;
}

bool IsMissingEntitlementError(NSError* error) {
  if (error == nil) return false;
  if (error.code == kSecMissingEntitlementStatus) return true;
  NSString* reason = [error localizedFailureReason];
  if (reason != nil && [reason rangeOfString:@"(-34018)"].location != NSNotFound) {
    return true;
  }
  NSString* desc = [error localizedDescription];
  if (desc != nil && [desc rangeOfString:@"(-34018)"].location != NSNotFound) {
    return true;
  }
  id underlying = error.userInfo[NSUnderlyingErrorKey];
  if ([underlying isKindOfClass:[NSError class]] && underlying != error) {
    return IsMissingEntitlementError(static_cast<NSError*>(underlying));
  }
  return false;
}

// Resolves a Security.framework CFStringRef query key via dlsym so targets that
// do not link Security.framework still compile and link cleanly. Falls back to
// the stable Apple Security ABI key literal if Security is not loaded.
NSString* LookupSecQueryKey(const char* symbol_name, NSString* fallback) {
  void* sym = dlsym(RTLD_DEFAULT, symbol_name);
  if (sym != nullptr) {
    CFStringRef cf_str = *reinterpret_cast<CFStringRef const*>(sym);
    if (cf_str != nullptr) {
      return (__bridge NSString*)cf_str;
    }
  }
  return fallback;
}

void UpdateHashWithOptionalValue(CC_SHA256_CTX* ctx, id value) {
  if (value == nil) {
    const uint8_t tag = 0;
    CC_SHA256_Update(ctx, &tag, sizeof(tag));
    return;
  }
  if ([value isKindOfClass:[NSString class]]) {
    const uint8_t tag = 1;
    NSData* utf8 = [static_cast<NSString*>(value) dataUsingEncoding:NSUTF8StringEncoding];
    const uint64_t len = static_cast<uint64_t>(utf8.length);
    CC_SHA256_Update(ctx, &tag, sizeof(tag));
    CC_SHA256_Update(ctx, &len, sizeof(len));
    if (len > 0) CC_SHA256_Update(ctx, utf8.bytes, static_cast<CC_LONG>(len));
    return;
  }
  if ([value isKindOfClass:[NSData class]]) {
    const uint8_t tag = 2;
    NSData* data = static_cast<NSData*>(value);
    const uint64_t len = static_cast<uint64_t>(data.length);
    CC_SHA256_Update(ctx, &tag, sizeof(tag));
    CC_SHA256_Update(ctx, &len, sizeof(len));
    if (len > 0) CC_SHA256_Update(ctx, data.bytes, static_cast<CC_LONG>(len));
    return;
  }
  NSString* desc = [value description] ?: @"";
  UpdateHashWithOptionalValue(ctx, desc);
}

NSString* SanitizeScopeComponent(NSString* raw) {
  if (raw == nil || raw.length == 0) return @"default";
  NSMutableString* out = [NSMutableString stringWithCapacity:raw.length];
  for (NSUInteger i = 0; i < raw.length && out.length < 128; ++i) {
    const unichar c = [raw characterAtIndex:i];
    const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '.' || c == '_' || c == '-';
    [out appendFormat:@"%c", safe ? static_cast<char>(c) : '_'];
  }
  if ([out isEqualToString:@"."] || [out isEqualToString:@".."] || out.length == 0) {
    return @"default";
  }
  return out;
}

NSString* FallbackDirectoryPath(bool create_if_missing, NSError* __autoreleasing* out_error) {
  NSArray<NSString*>* dirs =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
  NSString* base_dir = dirs.firstObject;
  if (base_dir == nil || base_dir.length == 0) {
    base_dir = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support"];
  }
  if (base_dir == nil || base_dir.length == 0) {
    base_dir = NSTemporaryDirectory();
  }
  NSString* scope = [[NSBundle mainBundle] bundleIdentifier];
  if (scope == nil || scope.length == 0) {
    scope = [[NSProcessInfo processInfo] processName];
  }
  NSString* root_dir =
      [base_dir stringByAppendingPathComponent:@"com.google.firebase.cpp.keychain"];
  NSString* scoped_dir = [root_dir stringByAppendingPathComponent:SanitizeScopeComponent(scope)];

  if (create_if_missing) {
    NSDictionary* attrs = @{NSFilePosixPermissions : @(0700)};
    NSError* dir_error = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:scoped_dir
                                   withIntermediateDirectories:YES
                                                    attributes:attrs
                                                         error:&dir_error]) {
      if (out_error != nullptr) *out_error = dir_error;
      return nil;
    }
    chmod([root_dir fileSystemRepresentation], 0700);
    chmod([scoped_dir fileSystemRepresentation], 0700);
  }
  return scoped_dir;
}

NSString* FallbackFilePathForQuery(NSDictionary* query, bool create_dir,
                                   NSError* __autoreleasing* out_error) {
  if (![query isKindOfClass:[NSDictionary class]]) return nil;
  // Never intercept queries pinned to an explicit SecKeychainRef.
  NSString* use_keychain_key = LookupSecQueryKey("kSecUseKeychain", @"u_KC");
  if (query[use_keychain_key] != nil) return nil;

  NSString* class_key = LookupSecQueryKey("kSecClass", @"class");
  NSString* service_key = LookupSecQueryKey("kSecAttrService", @"svce");
  NSString* account_key = LookupSecQueryKey("kSecAttrAccount", @"acct");
  NSString* access_group_key = LookupSecQueryKey("kSecAttrAccessGroup", @"agrp");
  NSString* generic_key = LookupSecQueryKey("kSecAttrGeneric", @"gena");

  id service = query[service_key];
  id account = query[account_key];
  if (service == nil && account == nil) return nil;

  NSString* dir = FallbackDirectoryPath(create_dir, out_error);
  if (dir == nil) return nil;

  CC_SHA256_CTX ctx;
  CC_SHA256_Init(&ctx);
  UpdateHashWithOptionalValue(&ctx, query[class_key]);
  UpdateHashWithOptionalValue(&ctx, service);
  UpdateHashWithOptionalValue(&ctx, account);
  UpdateHashWithOptionalValue(&ctx, query[access_group_key]);
  UpdateHashWithOptionalValue(&ctx, query[generic_key]);
  unsigned char digest[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256_Final(digest, &ctx);

  char hex[CC_SHA256_DIGEST_LENGTH * 2 + 1];
  static const char kHexDigits[] = "0123456789abcdef";
  for (size_t i = 0; i < CC_SHA256_DIGEST_LENGTH; ++i) {
    hex[i * 2] = kHexDigits[(digest[i] >> 4) & 0xF];
    hex[i * 2 + 1] = kHexDigits[digest[i] & 0xF];
  }
  hex[sizeof(hex) - 1] = '\0';

  NSString* filename = [NSString stringWithFormat:@"%s.bin", hex];
  return [dir stringByAppendingPathComponent:filename];
}

// Writes `data` atomically via mkstemp (born 0600 inside a 0700 directory) +
// rename so fallback tokens are never visible to other local users even
// transiently.
bool WriteFallbackFileAtomically(NSString* dest_path, NSData* data,
                                 NSError* __autoreleasing* out_error) {
  NSString* tmpl_str = [dest_path stringByAppendingString:@".tmp.XXXXXX"];
  const char* tmpl_cstr = [tmpl_str fileSystemRepresentation];
  std::vector<char> tmpl_buf(tmpl_cstr, tmpl_cstr + std::strlen(tmpl_cstr) + 1);

  const int fd = mkstemp(tmpl_buf.data());
  if (fd < 0) {
    const int err = errno;
    if (out_error != nullptr) {
      *out_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil];
    }
    return false;
  }
  (void)fchmod(fd, 0600);

  const auto* bytes = static_cast<const uint8_t*>(data.bytes);
  size_t remaining = static_cast<size_t>(data.length);
  while (remaining > 0) {
    const ssize_t written = write(fd, bytes, remaining);
    if (written < 0) {
      if (errno == EINTR) continue;
      const int err = errno;
      close(fd);
      unlink(tmpl_buf.data());
      if (out_error != nullptr) {
        *out_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil];
      }
      return false;
    }
    bytes += written;
    remaining -= static_cast<size_t>(written);
  }

  if (close(fd) != 0) {
    const int err = errno;
    unlink(tmpl_buf.data());
    if (out_error != nullptr) {
      *out_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil];
    }
    return false;
  }

  if (rename(tmpl_buf.data(), [dest_path fileSystemRepresentation]) != 0) {
    const int err = errno;
    unlink(tmpl_buf.data());
    if (out_error != nullptr) {
      *out_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil];
    }
    return false;
  }
  return true;
}

void LogFallbackOnce() {
  if (!g_logged_keychain_fallback) {
    g_logged_keychain_fallback = true;
    LogInfo("Firebase Apple desktop runtime: macOS Data Protection Keychain is "
            "unavailable without a provisioning entitlement (-34018); storing "
            "Firebase Installations credentials in user-only (0600) Application "
            "Support fallback storage.");
  }
}

NSData* SwizzledGetItem(id self, SEL _cmd, NSDictionary* query,
                        NSError* __autoreleasing* out_error) {
  NSError* kc_error = nil;
  NSData* data = g_orig_get_item(self, _cmd, query, &kc_error);
  if (g_in_original_set_item || data != nil) {
    if (out_error != nullptr) *out_error = data != nil ? nil : kc_error;
    return data;
  }

  // On macOS, an unprovisioned process calling SecItemCopyMatching with
  // kSecUseDataProtectionKeychain either fails with -34018 or reports
  // errSecItemNotFound (kc_error == nil). In both cases check fallback storage;
  // propagate any unrelated Keychain error untouched.
  const bool missing_entitlement = IsMissingEntitlementError(kc_error);
  if (kc_error != nil && !missing_entitlement) {
    if (out_error != nullptr) *out_error = kc_error;
    return nil;
  }

  @synchronized(self) {
    NSString* path = FallbackFilePathForQuery(query, /*create_dir=*/false, nil);
    if (path != nil && [[NSFileManager defaultManager] fileExistsAtPath:path]) {
      NSError* read_error = nil;
      NSData* fallback_data = [NSData dataWithContentsOfFile:path options:0 error:&read_error];
      if (fallback_data != nil) {
        if (out_error != nullptr) *out_error = nil;
        return fallback_data;
      }
      if (out_error != nullptr) *out_error = read_error;
      return nil;
    }
  }

  // No fallback file present: treat -34018 on lookup identically to item-not-
  // found so fresh installations proceed to create + save via setItem:.
  if (out_error != nullptr) *out_error = nil;
  return nil;
}

BOOL SwizzledSetItem(id self, SEL _cmd, NSData* item, NSDictionary* query,
                     NSError* __autoreleasing* out_error) {
  NSError* kc_error = nil;
  BOOL stored_in_keychain = NO;
  {
    ScopedOriginalSetItemFlag guard;
    stored_in_keychain = g_orig_set_item(self, _cmd, item, query, &kc_error);
  }
  if (stored_in_keychain) {
    // Provisioned Keychain write succeeded; clean up any stale fallback file.
    @synchronized(self) {
      NSString* path = FallbackFilePathForQuery(query, /*create_dir=*/false, nil);
      if (path != nil) {
        unlink([path fileSystemRepresentation]);
      }
    }
    if (out_error != nullptr) *out_error = nil;
    return YES;
  }

  if (!IsMissingEntitlementError(kc_error) || item == nil) {
    if (out_error != nullptr) *out_error = kc_error;
    return NO;
  }

  @synchronized(self) {
    NSError* file_error = nil;
    NSString* path = FallbackFilePathForQuery(query, /*create_dir=*/true, &file_error);
    if (path == nil || !WriteFallbackFileAtomically(path, item, &file_error)) {
      if (out_error != nullptr) *out_error = file_error ?: kc_error;
      return NO;
    }
    LogFallbackOnce();
  }
  if (out_error != nullptr) *out_error = nil;
  return YES;
}

BOOL SwizzledRemoveItem(id self, SEL _cmd, NSDictionary* query,
                        NSError* __autoreleasing* out_error) {
  NSError* unlink_error = nil;
  @synchronized(self) {
    NSString* path = FallbackFilePathForQuery(query, /*create_dir=*/false, nil);
    if (path != nil) {
      if (unlink([path fileSystemRepresentation]) != 0 && errno != ENOENT) {
        unlink_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
      }
    }
  }

  NSError* kc_error = nil;
  const BOOL kc_ok = g_orig_remove_item(self, _cmd, query, &kc_error);
  if (unlink_error != nil) {
    if (out_error != nullptr) *out_error = unlink_error;
    return NO;
  }
  if (kc_ok || IsMissingEntitlementError(kc_error)) {
    if (out_error != nullptr) *out_error = nil;
    return YES;
  }
  if (out_error != nullptr) *out_error = kc_error;
  return NO;
}

void EnsureKeychainFallbackInstalled() {
  Class gul_cls = NSClassFromString(@"GULKeychainUtils");
  if (!IsClassInCurrentImage(gul_cls)) return;

  @synchronized(gul_cls) {
    const SEL marker = sel_registerName("firebase_cpp_apple_desktop_keychain_fallback_v1");
    if (objc_getAssociatedObject(gul_cls, marker) != nil) return;

    Method get_m = class_getClassMethod(gul_cls, @selector(getItemWithQuery:error:));
    Method set_m = class_getClassMethod(gul_cls, @selector(setItem:withQuery:error:));
    Method rm_m = class_getClassMethod(gul_cls, @selector(removeItemWithQuery:error:));
    if (get_m == nullptr || set_m == nullptr || rm_m == nullptr) return;

    g_orig_get_item = reinterpret_cast<GetItemImp>(
        method_setImplementation(get_m, reinterpret_cast<IMP>(&SwizzledGetItem)));
    g_orig_set_item = reinterpret_cast<SetItemImp>(
        method_setImplementation(set_m, reinterpret_cast<IMP>(&SwizzledSetItem)));
    g_orig_remove_item = reinterpret_cast<RemoveItemImp>(
        method_setImplementation(rm_m, reinterpret_cast<IMP>(&SwizzledRemoveItem)));

    objc_setAssociatedObject(gul_cls, marker, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
}

void EnsurePromiseBackgroundQueueConfigured() {
  Class fbl_cls = NSClassFromString(@"FBLPromise");
  if (!IsClassInCurrentImage(fbl_cls)) return;
  if (![(id)fbl_cls respondsToSelector:@selector(defaultDispatchQueue)] ||
      ![(id)fbl_cls respondsToSelector:@selector(setDefaultDispatchQueue:)]) {
    return;
  }

  @synchronized(fbl_cls) {
    const SEL marker = sel_registerName("firebase_cpp_apple_desktop_fblpromise_queue_v1");
    if (objc_getAssociatedObject(fbl_cls, marker) != nil) return;

    auto typed_cls = (Class<FIRCppAppleDesktopPromiseClassShape>)fbl_cls;
    // Messaging typed_cls triggers +[FBLPromise initialize] first, which sets
    // defaultDispatchQueue to dispatch_get_main_queue(). Only replace it if it
    // is still the default main queue so explicit overrides stay untouched.
    if ([typed_cls defaultDispatchQueue] == dispatch_get_main_queue()) {
      dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(
          dispatch_queue_attr_make_with_autorelease_frequency(
              DISPATCH_QUEUE_SERIAL, DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM),
          QOS_CLASS_USER_INITIATED, 0);
      g_fbl_promise_queue = dispatch_queue_create("com.google.firebase.cpp.fblpromises", attr);
      [typed_cls setDefaultDispatchQueue:g_fbl_promise_queue];
    }

    objc_setAssociatedObject(fbl_cls, marker, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
}

}  // namespace

inline void EnsureRuntimeSupport() {
  @autoreleasepool {
    EnsurePromiseBackgroundQueueConfigured();
    EnsureKeychainFallbackInstalled();
  }
}

// Ensures the embedded GoogleAppMeasurement APMScreenViewReporter singleton has
// marked the process active so APMSessionReporter starts a session and serves
// sessionIDWithCompletion: even in headless desktop binaries or hosts (such as
// Unity Editor Play Mode) where NSApplicationDidBecomeActiveNotification fired
// before Firebase was configured. Must be called on the main thread.
inline void EnsureAnalyticsAppActiveStateOnMainThread() {
  @autoreleasepool {
    Class svr_cls = NSClassFromString(@"APMScreenViewReporter");
    if (!IsClassInCurrentImage(svr_cls)) return;
    if (![(id)svr_cls respondsToSelector:@selector(sharedInstance)]) return;

    auto typed_cls = (Class<FIRCppAppleDesktopScreenViewReporterClassShape>)svr_cls;
    id<FIRCppAppleDesktopScreenViewReporterShape> reporter = [typed_cls sharedInstance];
    if (reporter == nil) return;
    if (![(id)reporter respondsToSelector:@selector(isAppActive)] ||
        ![(id)reporter respondsToSelector:@selector(handleApplicationDidBecomeActive:)]) {
      return;
    }
    if (![reporter isAppActive]) {
      [reporter handleApplicationDidBecomeActive:nil];
    }
  }
}

}  // namespace apple_desktop
}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APPLE_DESKTOP_RUNTIME_INTERNAL_H_
