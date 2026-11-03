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

#ifndef FIREBASE_APP_SRC_UTIL_IOS_H_
#define FIREBASE_APP_SRC_UTIL_IOS_H_

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "app/src/include/firebase/internal/type_traits_ios.h"
#include "app/src/include/firebase/variant.h"

namespace firebase {
namespace util {

/// Define a class to wrap an Objective-C pointer under ARC.
/// Objective-C uses automatic reference counting, so we can't just cast
/// Objective-C object pointers to void* or a forward-declared C++ class to
/// reference them from platform-independent data structures (like
/// firebase::App's data_ pointer). Instead, we wrap the pointer in this class.
///
/// Usage:
///
///   // .h
///   struct MyPlatformIndependentClass {
///     void* platform_indep_ptr_;
///   };
///
///   // .mm
///   typedef ObjCPointer<MyObjCClass> MyObjCClassPointer;
///   // or OBJ_C_PTR_WRAPPER(MyObjCClass);
///   // or OBJ_C_PTR_WRAPPER_NAMED(MyObjCClassPointer, MyObjCClass);
///   // OBJ_C_PTR_WRAPPER* define a class rather than a typedef which
///   // is useful when defining a forward-declared class.
template <typename T>
class ObjCPointer {
 public:
  // Construct with nil.
  ObjCPointer() noexcept : objc_object_(nil) {}
  template <typename U,
            typename = std::enable_if_t<std::is_same_v<U, std::nullptr_t>>>
  // NOLINTNEXTLINE(runtime/explicit)
  ObjCPointer(U) noexcept : objc_object_(nil) {}

  // Construct with a reference to an Obj-C object.
  explicit ObjCPointer(T *_Nullable objc_object) noexcept
      : objc_object_(objc_object) {}

  ObjCPointer(const ObjCPointer &other) noexcept
      : objc_object_(other.objc_object_) {}

  ObjCPointer(ObjCPointer &&other) noexcept
      : objc_object_(other.objc_object_) {
    other.objc_object_ = nil;
  }

  // Release the strong ARC reference directly without placing the object into
  // the surrounding autorelease pool (fixes BUG-0378).
  ~ObjCPointer() { objc_object_ = nil; }

  // Determine whether the Obj-C object is valid.
  explicit operator bool() const noexcept { return objc_object_ != nil; }

  // Get the Obj-C object.
  T *_Nullable operator*() const noexcept { return objc_object_; }

  // Get the Obj-C object.
  T *_Nullable get() const noexcept { return objc_object_; }

  // Release the reference to the Obj-C object and return it.
  T *_Nullable release() noexcept {
    T *released = objc_object_;
    objc_object_ = nil;
    return released;
  }

  // Assign a new Obj-C object.
  void reset(T *_Nullable objc_object = nil) noexcept {
    objc_object_ = objc_object;
  }

  // Assign a new Obj-C object.
  ObjCPointer &operator=(T *_Nullable objc_object) noexcept {
    objc_object_ = objc_object;
    return *this;
  }

  ObjCPointer &operator=(std::nullptr_t) noexcept {
    objc_object_ = nil;
    return *this;
  }

  ObjCPointer &operator=(const ObjCPointer &other) noexcept {
    if (this != &other) {
      objc_object_ = other.objc_object_;
    }
    return *this;
  }

  ObjCPointer &operator=(ObjCPointer &&other) noexcept {
    if (this != &other) {
      objc_object_ = other.objc_object_;
      other.objc_object_ = nil;
    }
    return *this;
  }

  // Get the Obj-C object from an ObjCPointer if the specified wrapper is
  // non-null.
  static T *_Nullable SafeGet(const ObjCPointer *_Nullable reference) noexcept {
    return reference ? reference->get() : nil;
  }

 private:
  T *__strong _Nullable objc_object_;
};

template <typename T>
using ObjcPointer = ObjCPointer<T>;

/// Thread-safe reference wrapper for capturing a C++ object pointer inside
/// asynchronous Objective-C completion blocks without risking use-after-free
/// when the C++ owner is destroyed while the block is in flight.
///
/// The C++ owner holds a `SafeReference<MyClass>` member and calls
/// `Invalidate()` in its destructor. Objective-C blocks capture a copy of the
/// `SafeReference<MyClass>` by value and call `Run([](MyClass* self) { ... })`
/// or `auto locked = safe_ref.Lock();`.
template <typename T>
class SafeReference {
 private:
  struct State {
    explicit State(T *_Nullable p) : ptr(p) {}
    mutable std::recursive_mutex mutex;
    T *_Nullable ptr;
  };

 public:
  class LockedRef {
   public:
    LockedRef() noexcept : ptr_(nullptr) {}
    LockedRef(std::shared_ptr<State> state,
              std::unique_lock<std::recursive_mutex> &&lock,
              T *_Nullable ptr) noexcept
        : state_(std::move(state)), lock_(std::move(lock)), ptr_(ptr) {}

    LockedRef(LockedRef &&other) noexcept
        : state_(std::move(other.state_)),
          lock_(std::move(other.lock_)),
          ptr_(other.ptr_) {
      other.ptr_ = nullptr;
    }

    LockedRef &operator=(LockedRef &&other) noexcept {
      if (this != &other) {
        if (lock_.owns_lock()) {
          lock_.unlock();
        }
        lock_ = std::move(other.lock_);
        state_ = std::move(other.state_);
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
      }
      return *this;
    }

    LockedRef(const LockedRef &) = delete;
    LockedRef &operator=(const LockedRef &) = delete;

    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    T *_Nullable get() const noexcept { return ptr_; }
    T *_Nullable operator->() const noexcept { return ptr_; }
    T &operator*() const noexcept { return *ptr_; }

   private:
    // `state_` MUST be declared before `lock_` so `State` (and `State::mutex`)
    // remains alive until after `~LockedRef()` unlocks `lock_`.
    std::shared_ptr<State> state_;
    std::unique_lock<std::recursive_mutex> lock_;
    T *_Nullable ptr_;
  };

  SafeReference() noexcept = default;
  explicit SafeReference(T *_Nullable ptr)
      : state_(ptr ? std::make_shared<State>(ptr) : nullptr) {}

  SafeReference(const SafeReference &) = default;
  SafeReference(SafeReference &&) noexcept = default;
  SafeReference &operator=(const SafeReference &) = default;
  SafeReference &operator=(SafeReference &&) noexcept = default;

  // Invalidates the reference under the lock. Any currently executing `Run()`
  // or `Lock()` scope completes before `Invalidate()` returns, and any
  // subsequent block invocation becomes a safe no-op.
  void Invalidate() const {
    if (!state_) return;
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    state_->ptr = nullptr;
  }

  void Clear() const { Invalidate(); }

  void Reset(T *_Nullable ptr = nullptr) {
    if (!state_) {
      if (ptr) {
        state_ = std::make_shared<State>(ptr);
      }
      return;
    }
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    state_->ptr = ptr;
  }

  LockedRef Lock() const {
    std::shared_ptr<State> state = state_;
    if (!state) return LockedRef();
    std::unique_lock<std::recursive_mutex> lock(state->mutex);
    if (!state->ptr) {
      return LockedRef();
    }
    T *ptr = state->ptr;
    return LockedRef(std::move(state), std::move(lock), ptr);
  }

  template <typename Fn>
  bool Run(Fn &&fn) const {
    LockedRef locked = Lock();
    if (!locked) return false;
    std::forward<Fn>(fn)(locked.get());
    return true;
  }

 private:
  std::shared_ptr<State> state_;
};

template <typename T>
SafeReference<T> MakeSafeReference(T *_Nullable ptr) {
  return SafeReference<T>(ptr);
}

/// Block capture helper that supports both `std::shared_ptr<T>` /
/// `std::weak_ptr<T>` and `SafeReference<T>` owners in Objective-C blocks.
template <typename T>
class BlockWeakSelf {
 public:
  BlockWeakSelf() noexcept = default;

  // NOLINTNEXTLINE(runtime/explicit)
  BlockWeakSelf(const std::shared_ptr<T> &shared) noexcept
      : weak_ptr_(shared) {}
  // NOLINTNEXTLINE(runtime/explicit)
  BlockWeakSelf(const std::weak_ptr<T> &weak) noexcept : weak_ptr_(weak) {}
  // NOLINTNEXTLINE(runtime/explicit)
  BlockWeakSelf(const SafeReference<T> &safe_ref) noexcept
      : safe_ref_(safe_ref), uses_safe_ref_(true) {}

  std::shared_ptr<T> LockShared() const noexcept { return weak_ptr_.lock(); }

  typename SafeReference<T>::LockedRef LockSafeRef() const {
    return safe_ref_.Lock();
  }

  template <typename Fn>
  bool Run(Fn &&fn) const {
    if (uses_safe_ref_) {
      return safe_ref_.Run(std::forward<Fn>(fn));
    }
    if (std::shared_ptr<T> locked = weak_ptr_.lock()) {
      std::forward<Fn>(fn)(locked.get());
      return true;
    }
    return false;
  }

 private:
  std::weak_ptr<T> weak_ptr_;
  SafeReference<T> safe_ref_;
  bool uses_safe_ref_ = false;
};

template <typename T>
BlockWeakSelf<T> MakeBlockWeakSelf(const std::shared_ptr<T> &shared) noexcept {
  return BlockWeakSelf<T>(shared);
}

template <typename T>
BlockWeakSelf<T> MakeBlockWeakSelf(const std::weak_ptr<T> &weak) noexcept {
  return BlockWeakSelf<T>(weak);
}

template <typename T>
BlockWeakSelf<T> MakeBlockWeakSelf(const SafeReference<T> &safe_ref) noexcept {
  return BlockWeakSelf<T>(safe_ref);
}

// Generate the class named class_name as an alias of ObjCPointer to contain
// the Objective-C type objc_type_name.
#define OBJ_C_PTR_WRAPPER_NAMED(class_name, objc_type_name)                  \
  class class_name : public firebase::util::ObjCPointer<objc_type_name> {    \
   public:                                                                   \
    class_name() noexcept {}                                                 \
    template <typename U,                                                    \
              typename = std::enable_if_t<std::is_same_v<U, std::nullptr_t>>> \
    class_name(U) noexcept                                                   \
        : firebase::util::ObjCPointer<objc_type_name>(nil) {}                \
    explicit class_name(                                                     \
        const firebase::util::ObjCPointer<objc_type_name> &obj) noexcept     \
        : firebase::util::ObjCPointer<objc_type_name>(obj) {}                \
    explicit class_name(objc_type_name *_Nullable objc_object) noexcept      \
        : firebase::util::ObjCPointer<objc_type_name>(objc_object) {}        \
    class_name(const class_name &) noexcept = default;                       \
    class_name(class_name &&) noexcept = default;                            \
    class_name &operator=(const class_name &) noexcept = default;            \
    class_name &operator=(class_name &&) noexcept = default;                 \
    class_name &operator=(objc_type_name *_Nullable objc_object) noexcept {  \
      ObjCPointer<objc_type_name>::operator=(objc_object);                   \
      return *this;                                                          \
    }                                                                        \
    class_name &operator=(std::nullptr_t) noexcept {                         \
      ObjCPointer<objc_type_name>::operator=(nullptr);                       \
      return *this;                                                          \
    }                                                                        \
  }

/// Return an std::string created from an NSString pointer.
/// If the NSString pointer is nil or cannot be encoded as UTF-8, returns an
/// empty string (never passes nullptr to std::string, fixing BUG-0145).
/// Preserves embedded NUL bytes when present.
inline std::string StringFromNSString(NSString *_Nullable ns_string) {
  if (!ns_string || [ns_string length] == 0) return std::string();
  NSUInteger utf8_length =
      [ns_string lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  if (utf8_length > 0) {
    std::string result(utf8_length, '\0');
    NSUInteger used_length = 0;
    NSRange full_range = NSMakeRange(0, [ns_string length]);
    if ([ns_string getBytes:&result[0]
                  maxLength:utf8_length
                 usedLength:&used_length
                   encoding:NSUTF8StringEncoding
                    options:0
                      range:full_range
             remainingRange:NULL]) {
      result.resize(used_length);
      return result;
    }
  }
  const char *utf8 = [ns_string UTF8String];
  return utf8 ? std::string(utf8) : std::string();
}

/// Return an std::string created from an NSURL pointer.
/// If the NSURL pointer is nil, returns a string of length zero.
inline std::string StringFromNSUrl(NSURL *_Nullable url) {
  return url ? StringFromNSString(url.absoluteString) : std::string();
}

/// Structured representation of an NSError translated into C++.
struct ErrorInfo {
  int code = 0;
  std::string domain;
  std::string message;
};

/// Extracts a safe UTF-8 error message from an NSError, falling back to
/// `default_message` (or "Unknown error") if `error` or `localizedDescription`
/// is nil.
inline std::string NSErrorToErrorMessage(
    NSError *_Nullable error,
    const char *_Nullable default_message = "Unknown error") {
  if (!error) return std::string();
  NSString *desc = [error localizedDescription];
  if (desc) {
    std::string msg = StringFromNSString(desc);
    if (!msg.empty()) return msg;
  }
  return default_message ? std::string(default_message) : std::string();
}

/// Translates an `NSError*` into a `firebase::util::ErrorInfo` with safe
/// UTF-8 domain and message strings.
inline ErrorInfo TranslateNSError(
    NSError *_Nullable error, int default_error_code = -1,
    const char *_Nullable default_message = "Unknown error") {
  ErrorInfo info;
  if (!error) {
    info.code = 0;
    return info;
  }
  info.code = static_cast<int>([error code]);
  if (info.code == 0 && default_error_code != 0) {
    info.code = default_error_code;
  }
  info.domain = StringFromNSString([error domain]);
  info.message = NSErrorToErrorMessage(error, default_message);
  return info;
}

/// Looks up `[error code]` in `code_map` (e.g., `std::map<NSInteger, Error>`
/// or `std::unordered_map<int, Error>`), returning `default_error_code` if
/// not found, and optionally populates `*out_message`.
template <typename CodeMap, typename ErrorCodeType>
ErrorCodeType TranslateNSErrorCode(
    NSError *_Nullable error, const CodeMap &code_map,
    ErrorCodeType default_error_code, ErrorCodeType ok_code,
    std::string *_Nullable out_message = nullptr) {
  if (!error) {
    if (out_message) out_message->clear();
    return ok_code;
  }
  if (out_message) {
    *out_message = NSErrorToErrorMessage(error);
  }
  auto it = code_map.find(static_cast<typename CodeMap::key_type>([error code]));
  if (it != code_map.end()) {
    return static_cast<ErrorCodeType>(it->second);
  }
  return default_error_code;
}

NS_ASSUME_NONNULL_BEGIN

// C function typedef's of methods in AppDelegate which can be replaced by
// your library.
typedef BOOL (*AppDelegateApplicationDidFinishLaunchingWithOptionsFunc)(
    id self, SEL selector_value, UIApplication *application,
    NSDictionary *_Nullable launch_options);
typedef void (*AppDelegateApplicationDidBecomeActiveFunc)(
    id self, SEL selector_value, UIApplication *application);
typedef void (*AppDelegateApplicationDidEnterBackgroundFunc)(
    id self, SEL selector_value, UIApplication *application);
typedef void (
    *AppDelegateApplicationDidRegisterForRemoteNotificationsWithDeviceTokenFunc)(
    id self, SEL selector_value, UIApplication *application,
    NSData *deviceToken);
typedef void (
    *AppDelegateApplicationDidFailToRegisterForRemoteNotificationsWithErrorFunc)(
    id self, SEL selector_value, UIApplication *application, NSError *error);
typedef void (*AppDelegateApplicationDidReceiveRemoteNotificationFunc)(
    id self, SEL selector_value, UIApplication *application,
    NSDictionary *user_info);
typedef void (^UIBackgroundFetchResultFunction)(UIBackgroundFetchResult result);
typedef void (
    *AppDelegateApplicationDidReceiveRemoteNotificationFetchCompletionHandlerFunc)(
    id self, SEL selector_value, UIApplication *application,
    NSDictionary *user_info, UIBackgroundFetchResultFunction handler);
typedef BOOL (*AppDelegateApplicationOpenUrlSourceApplicationAnnotationFunc)(
    id self, SEL selector_value, UIApplication *application, NSURL *url,
    NSString *_Nullable sourceApplication, id _Nullable annotation);

typedef BOOL (*AppDelegateApplicationOpenUrlOptionsFunc)(
    id self, SEL selector_value, UIApplication *application, NSURL *url,
    NSDictionary *options);

typedef BOOL (
    *AppDelegateApplicationContinueUserActivityRestorationHandlerFunc)(
    id self, SEL selector_value, UIApplication *application,
    NSUserActivity *user_activity,
    void (^restoration_handler)(NSArray *_Nullable));

// Calls the given block for each unique Objective-C class that has been
// previously passed to [UIApplication setDelegate:]. The block is executed
// immediately for all currently known unique delegate classes.
// Additionally, the block is queued to be executed if any new, unique
// Objective-C class is passed to [UIApplication setDelegate:] in the future.
void RunOnAppDelegateClasses(void (^block)(Class));

// Convert a string array into an NSMutableArray.
NSMutableArray *StringVectorToNSMutableArray(
    const std::vector<std::string> &vector);

// Convert an unordered_set of strings into an NSMutableArray.
NSMutableArray *StringUnorderedSetToNSMutableArray(
    const std::unordered_set<std::string> &set);

// Convert an NSArray into a vector of strings. Asserts if a non-NSString
// object is found in the array.
void NSArrayOfNSStringToVectorOfString(NSArray *_Nullable array,
                                       std::vector<std::string> *string_vector);

// Convert a string map to NSDictionary.
NSDictionary *StringMapToNSDictionary(
    const std::map<std::string, std::string> &string_map);

// Convert a const char * map to NSDictionary.
NSDictionary *CharArrayMapToNSDictionary(
    const std::map<const char *, const char *> &string_map);

// Convert a std::string to NSData.
NSData *StringToNSData(const std::string &str);

// Convert bytes to NSData.
NSData *BytesToNSData(const char *_Nullable bytes, const int len);

// Convert bytes to NSData.
NSData *BytesToNSData(const unsigned char *_Nullable bytes, const int len);

// Convert bytes to NSData with size_t length (avoids 32-bit int truncation,
// fixing BUG-0379).
NSData *BytesToNSData(const void *_Nullable bytes, size_t len);

// Convert an NSData to std::string.
std::string NSDataToString(NSData *_Nullable data);

// Convert a std::string to NSString (never returns nil; returns @"" on invalid
// UTF-8, and preserves embedded NUL bytes).
NSString *StringToNSString(const std::string &str);

// Convert a C-string to NSString (returns @"" if c_str is nullptr or invalid
// UTF-8, fixing nil insertion crashes in Objective-C collections).
NSString *CStringToNSString(const char *_Nullable c_str);

// Convert an NSString to std::string (returns "" if string is nil, fixing
// BUG-0145).
std::string NSStringToString(NSString *_Nullable string);

// Convert a vector<Variant> into an NSMutableArray.
NSMutableArray *StdVectorToNSMutableArray(const std::vector<Variant> &vector);

// Convert a map<Variant, Variant> into an NSMutableDictionary.
NSMutableDictionary *StdMapToNSMutableDictionary(
    const std::map<Variant, Variant> &map);

// Convert an NSArray into a vector<Variant>.
void NSArrayToStdVector(NSArray *_Nullable array,
                        std::vector<Variant> *vector);

// Convert a Variant to an NSObject.
id VariantToId(const Variant &variant);

// Convert an NSObject to a Variant.
Variant IdToVariant(id _Nullable value);

// Converts an NSDictionary mapping id-to-id to a map<Variant, Variant>.
void NSDictionaryToStdMap(NSDictionary *_Nullable dictionary,  // NOLINT
                          std::map<Variant, Variant> *map);

// Runs a block on the main/UI thread immediately if the current thread is the
// main thread; otherwise, dispatches asynchronously to the main thread.
void DispatchAsyncSafeMainQueue(void (^block)(void));

// Runs a C++ function on the main/UI thread.
void RunOnMainThread(void (*function_ptr)(void *_Nullable function_data),
                     void *_Nullable function_data);

// Runs a C++ function on a background thread.
void RunOnBackgroundThread(void (*function_ptr)(void *_Nullable function_data),
                           void *_Nullable function_data);

// Class which caches function implementations for a class.
//
// Use this object to swizzle and cache method implementations.
class ClassMethodImplementationCache {
 public:
  ClassMethodImplementationCache();
  ~ClassMethodImplementationCache();

  // Replaces an existing method implementation on a class, caching the
  // original implementation or adding the method to the class if it doesn't
  // exist.
  //
  // If we need to add the method, we will use the type encoding of the
  // selector |name| on |type_encoding_class|.  The |type_encoding_class| is
  // only used if a method isn't found for the specified selector |name| and
  // therefore a new method needs to be added to the class.
  //
  // The cached method implementation can be retrieved using GetMethod().
  void ReplaceOrAddMethod(Class clazz, SEL name, IMP imp,
                          Class type_encoding_class) {
    ReplaceOrAddMethod(clazz, name, imp, type_encoding_class, true);
  }

  // Replace a method on a class caching the original method implementation.
  // If an implementation for the specified selector does not exist, this
  // function does not modify the class.
  // See ReplaceOrAddMethod() for more details.
  void ReplaceMethod(Class clazz, SEL name, IMP imp,
                     Class type_encoding_class) {
    ReplaceOrAddMethod(clazz, name, imp, type_encoding_class, false);
  }

  // Get the original method implementation for the specific selector.
  IMP _Nullable GetMethod(Class _Nullable clazz, SEL name);
  IMP _Nullable GetMethodForObject(id _Nullable self, SEL name) {
    return self ? GetMethod([self class], name) : nil;
  }

  // Get or create a cache object.
  // This simplifies the creation of a new cache object before C++ static
  // constructors are executed.
  static ClassMethodImplementationCache *_Nullable GetCreateCache(
      ClassMethodImplementationCache *_Nullable *_Nullable cache);

 private:
  // Replace or add a method (if add_method is true) to a class.
  // See ReplaceOrAddMethod() for more details.
  void ReplaceOrAddMethod(Class clazz, SEL name, IMP imp,
                          Class type_encoding_class, bool add_method);

  void SetMethod(SEL name, Class clazz, NSString *implementation_selector_name);

  // Generate a random method name from the specified selector name. This is
  // used to store the implementation of an overridden method.
  NSString *_Nullable GenerateRandomSelectorName(SEL name);

 private:
  // Protects concurrent swizzling and lookup operations.
  mutable std::mutex mutex_;

  // Dictionary which contains the implementation (IMP) selector names of
  // swizzled methods that respond to each selector that is swizzled.
  // Indexed by original selector name -> NSMutableDictionary mapping
  // class name (NSStringFromClass(clazz)) -> random implementation selector
  // name.
  NSMutableDictionary *selector_implementation_names_per_selector_;

  // Number of times to attempt to generate a random selector name.
  static const int kRandomNameGenerationRetries;
};

NS_ASSUME_NONNULL_END

}  // namespace util

namespace apple {
using util::BlockWeakSelf;
using util::ErrorInfo;
using util::MakeBlockWeakSelf;
using util::MakeSafeReference;
using util::NSErrorToErrorMessage;
using util::ObjCPointer;
using util::ObjcPointer;
using util::SafeReference;
using util::StringFromNSString;
using util::StringFromNSUrl;
using util::TranslateNSError;
using util::TranslateNSErrorCode;
}  // namespace apple

using util::ObjCPointer;
using util::ObjcPointer;

}  // namespace firebase

// Define a sample UI ApplicationDelegate simply to retrieve the method
// encodings from the class, for when we need to add a new method instead of
// setting the existing method implementation on an app delegate. You can use
// this as the type encoding class for ReplaceOrAddMethod, if you are
// modifying methods on an app delegate class.
@interface FIRSAMAppDelegate : UIResponder <UIApplicationDelegate>
@end

#else

// Define an opaque type for the Obj-C pointer wrapper struct when this header
// is included in plain C++ files.
#define OBJ_C_PTR_WRAPPER_NAMED(class_name, objc_type_name) class class_name

#endif  // __OBJC__

// Generate the class definition objc_type_name##Pointer which is a container
// of the Objective-C type objc_type_name.
// For more information, see ObjCPointer.
#define OBJ_C_PTR_WRAPPER(objc_type_name) \
  OBJ_C_PTR_WRAPPER_NAMED(objc_type_name##Pointer, objc_type_name)

#endif  // FIREBASE_APP_SRC_UTIL_IOS_H_
