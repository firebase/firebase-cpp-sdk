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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_UTIL_IOS_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_UTIL_IOS_H_

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <map>
#include <string>

#include "app/src/include/firebase/variant.h"

namespace firebase {
namespace util {

/// Define a class to wrap an Objective-C pointer.
/// Objective-C uses automatic reference counting, we can't just cast them to
/// void* or a forward declared C++ class to reference them from
//  platform-independent data structures (like firebase::App's data_ pointer).
/// Instead, we wrap the pointer in this class.
///
/// Usage:
///
///   // .h
///   struct MyPlatformIndependentClass {
///     void* platform_indep_ptr_;
///   };
///
///   // .cpp
///   typedef ObjCPointer<MyObjCClass> MyObjCClassPointer;
///   // or OBJ_C_PTR_WRAPPER(MyObjCClass);
///   // or OBJ_C_PTR_WRAPPER_NAMED(MyObjCClassPointer, MyObjCClass);
//    // OBJ_C_PTR_WRAPPER* define a class rather than a typedef which
//    // is useful when defining a forward declared class.
///
///   void Init(MyPlatformIndependentClass* c, MyObjCClass* obj_c) {
///     c->platform_indep_ptr_ = new MyObjCClassPointer(obj_c);
///   }
///
///   void Uninit(MyPlatformIndependentClass* c) {
///     delete static_cast<MyObjCClassPointer*>(c->platform_indep_ptr_);
///     c->platform_indep_ptr_ = nullptr;
///   }
///
///   void DoSomething(MyPlatformIndependentClass* c) {
///     [static_cast<MyObjCClassPointer*>(
///        c->platform_indep_ptr_)->get() fn_name];
///   }
///
template <typename T>
class ObjCPointer {
 public:
  // Construct with an empty class.
  ObjCPointer() : objc_object_(nil) {}

  // Construct with a reference to an Obj-C object.
  explicit ObjCPointer(T *_Nullable objc_object) : objc_object_(objc_object) {}

  // Release the reference to the Obj-C object.
  ~ObjCPointer() { release(); }

  // Determine whether the Obj-C object is valid.
  explicit operator bool() const { return get() != nil; }

  // Get the Obj-C object.
  T *_Nullable operator*() const { return get(); }

  // Get the Obj-C object.
  T *_Nullable get() const { return objc_object_; }

  // Release the reference to the Obj-C object.
  T *_Nullable release() {
    T *released = objc_object_;
    objc_object_ = nil;
    return released;
  }

  // Assign a new Obj-C object.
  void reset(T *_Nullable objc_object) { objc_object_ = objc_object; }

  // Assign a new Obj-C object.
  ObjCPointer &operator=(T *_Nullable objc_object) {
    reset(objc_object);
    return *this;
  }

  // Get the Obj-C object from an ObjCPointer if the specified object is
  // non-null.
  static T *_Nullable SafeGet(const ObjCPointer *_Nullable reference) {
    return reference ? reference->get() : nil;
  }

 private:
  /* This should be private */
  T *_Nullable objc_object_;
};

// Generate the class named class_name as an alias of ObjCPointer to contain
// the Objective-C type objc_type_name.
#define OBJ_C_PTR_WRAPPER_NAMED(class_name, objc_type_name)               \
  class class_name : public firebase::util::ObjCPointer<objc_type_name> { \
   public:                                                                \
    class_name() {}                                                       \
    explicit class_name(                                                  \
        const firebase::util::ObjCPointer<objc_type_name>& obj)           \
        : firebase::util::ObjCPointer<objc_type_name>(obj) {}             \
    explicit class_name(objc_type_name *_Nullable objc_object)            \
        : firebase::util::ObjCPointer<objc_type_name>(objc_object) {}     \
    class_name &operator=(objc_type_name *_Nullable objc_object) {        \
      ObjCPointer<objc_type_name>::operator=(objc_object);                \
      return *this;                                                       \
    }                                                                     \
  }

/// Return an std::string created from an NSString pointer.
/// If the NSString pointer is nil, returns a string of length zero.
static inline std::string StringFromNSString(NSString *_Nullable ns_string) {
  return std::string(ns_string ? [ns_string UTF8String] : "");
}

/// Return an std::string created from an NSURL pointer.
/// If the NSURL pointer is nil, returns a string of length zero.
static inline std::string StringFromNSUrl(NSURL *_Nullable url) {
  return url ? StringFromNSString(url.absoluteString) : "";
}

NS_ASSUME_NONNULL_BEGIN

// C function typedef's of methods in AppDelegate which can be replaced by
// your library.
typedef BOOL (*AppDelegateApplicationDidFinishLaunchingWithOptionsFunc)(
    id self, SEL selector_value, UIApplication *application,
    NSDictionary *launch_options);
typedef BOOL (*AppDelegateApplicationDidFinishLaunchingWithOptionsFunc)(
    id self, SEL selector_value, UIApplication *application,
    NSDictionary *launch_options);
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
    NSString *sourceApplication, id annotation);

typedef BOOL (*AppDelegateApplicationOpenUrlOptionsFunc)(
    id self, SEL selector_value, UIApplication *application, NSURL *url,
    NSDictionary *options);

typedef BOOL (
    *AppDelegateApplicationContinueUserActivityRestorationHandlerFunc)(
    id self, SEL selector_value, UIApplication *application,
    NSUserActivity *user_activity, void (^restoration_handler)(NSArray *));

// Call the given block once for every Objective-C class that exists that
// implements the UIApplicationDelegate protocol (except for those in a
// blacklist we keep).
void ForEachAppDelegateClass(void (^block)(Class));

// Convert a string array into an NSMutableArray.
NSMutableArray *StringVectorToNSMutableArray(
    const std::vector<std::string> &vector);

// Convert a string map to NSDictionary.
NSDictionary *StringMapToNSDictionary(
    const std::map<std::string, std::string> &string_map);

// Convert a const char * map to NSDictionary.
NSDictionary *CharArrayMapToNSDictionary(
    const std::map<const char *, const char *> &string_map);

// Convert a std::string to NSData.
NSData *StringToNSData(const std::string &str);

// Convert bytes to NSData.
NSData *BytesToNSData(const char *bytes, const int len);

// Convert an NSData to std::string.
std::string NSDataToString(NSData *data);

// Convert a std::string to NSString.
NSString *StringToNSString(const std::string &str);

// Convert a C-string to NSString.
NSString *CStringToNSString(const char *c_str);

// Convert an NSString to std::string.
std::string NSStringToString(NSString *string);

// Convert a Variant to an NSObject.
id VariantToId(const Variant &variant);

// Convert an NSObject to a Variant.
Variant IdToVariant(id value);

// Converts an NSMutableDictionary mapping id-to-id to a map<Variant, Variant>.
void NSDictionaryToStdMap(NSDictionary *dictionary,  // NOLINT
                          std::map<Variant, Variant> *map);

// Runs a block on the main/UI thread immediately if the current thread is the
// main thread; otherwise, dispatches asynchronously to the main thread.
void DispatchAsyncSafeMainQueue(void (^block)(void));

// Runs a C++ function on the main/UI thread.
void RunOnMainThread(void (*function_ptr)(void *function_data),
                     void *function_data);

// Runs a C++ function on a background thread.
void RunOnBackgroundThread(void (*function_ptr)(void *function_data),
                           void *function_data);

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
  // The cached method method implementation can be retrieved using GetMethod().
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
  IMP GetMethod(Class clazz, SEL name);
  IMP GetMethodForObject(id self, SEL name) {
    return GetMethod([self class], name);
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

  void SetMethod(SEL name, NSString *implementation_selector_name);

  // Generate a random method name from the specified selector name.  This is
  // used to store the implementation of an overridden method.
  NSString *GenerateRandomSelectorName(SEL name);

 private:
  // Dictionary which contains the implementation (IMP) of swizzled methods
  // that respond to each selector that is swizzled.  The dictionary is
  // indexed by selector name with each element that referencing a dictionary
  // of potential names for the selector that contains the original method
  // implementation on the class.
  // i.e
  // selector_implementation_names_dict = dict[original_selector_name];
  NSMutableDictionary *selector_implementation_names_per_selector_;

  // Number of times to attempt to generate a random selector name.
  static const int kRandomNameGenerationRetries;
};

NS_ASSUME_NONNULL_END

}  // namespace util
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
#define OBJ_C_PTR_WRAPPER_NAMED(class_name, objc_type_name) class class_name;

#endif  // __OBJC__

// Generate the class definition objc_type_name##Pointer which is a container
// of the Objective-C type objc_type_name.
// For more information, see ObjCPointer.
#define OBJ_C_PTR_WRAPPER(objc_type_name) \
  OBJ_C_PTR_WRAPPER_NAMED(objc_type_name##Pointer, objc_type_name)

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_UTIL_IOS_H_
