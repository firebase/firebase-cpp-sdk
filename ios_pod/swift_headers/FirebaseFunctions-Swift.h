// Copyright 2022 Google LLC
// Copied from Firebase iOS SDK 9.5.0.

// Generated by Apple Swift version 5.6 (swiftlang-5.6.0.323.62 clang-1316.0.20.8)
#ifndef FIREBASEFUNCTIONS_SWIFT_H
#define FIREBASEFUNCTIONS_SWIFT_H
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgcc-compat"

#if !defined(__has_include)
#define __has_include(x) 0
#endif
#if !defined(__has_attribute)
#define __has_attribute(x) 0
#endif
#if !defined(__has_feature)
#define __has_feature(x) 0
#endif
#if !defined(__has_warning)
#define __has_warning(x) 0
#endif

#if __has_include(<swift/objc-prologue.h>)
#include <swift/objc-prologue.h>
#endif

#pragma clang diagnostic ignored "-Wauto-import"
#include <Foundation/Foundation.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(SWIFT_TYPEDEFS)
#define SWIFT_TYPEDEFS 1
#if __has_include(<uchar.h>)
#include <uchar.h>
#elif !defined(__cplusplus)
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#endif
typedef float swift_float2 __attribute__((__ext_vector_type__(2)));
typedef float swift_float3 __attribute__((__ext_vector_type__(3)));
typedef float swift_float4 __attribute__((__ext_vector_type__(4)));
typedef double swift_double2 __attribute__((__ext_vector_type__(2)));
typedef double swift_double3 __attribute__((__ext_vector_type__(3)));
typedef double swift_double4 __attribute__((__ext_vector_type__(4)));
typedef int swift_int2 __attribute__((__ext_vector_type__(2)));
typedef int swift_int3 __attribute__((__ext_vector_type__(3)));
typedef int swift_int4 __attribute__((__ext_vector_type__(4)));
typedef unsigned int swift_uint2 __attribute__((__ext_vector_type__(2)));
typedef unsigned int swift_uint3 __attribute__((__ext_vector_type__(3)));
typedef unsigned int swift_uint4 __attribute__((__ext_vector_type__(4)));
#endif

#if !defined(SWIFT_PASTE)
#define SWIFT_PASTE_HELPER(x, y) x##y
#define SWIFT_PASTE(x, y) SWIFT_PASTE_HELPER(x, y)
#endif
#if !defined(SWIFT_METATYPE)
#define SWIFT_METATYPE(X) Class
#endif
#if !defined(SWIFT_CLASS_PROPERTY)
#if __has_feature(objc_class_property)
#define SWIFT_CLASS_PROPERTY(...) __VA_ARGS__
#else
#define SWIFT_CLASS_PROPERTY(...)
#endif
#endif

#if __has_attribute(objc_runtime_name)
#define SWIFT_RUNTIME_NAME(X) __attribute__((objc_runtime_name(X)))
#else
#define SWIFT_RUNTIME_NAME(X)
#endif
#if __has_attribute(swift_name)
#define SWIFT_COMPILE_NAME(X) __attribute__((swift_name(X)))
#else
#define SWIFT_COMPILE_NAME(X)
#endif
#if __has_attribute(objc_method_family)
#define SWIFT_METHOD_FAMILY(X) __attribute__((objc_method_family(X)))
#else
#define SWIFT_METHOD_FAMILY(X)
#endif
#if __has_attribute(noescape)
#define SWIFT_NOESCAPE __attribute__((noescape))
#else
#define SWIFT_NOESCAPE
#endif
#if __has_attribute(ns_consumed)
#define SWIFT_RELEASES_ARGUMENT __attribute__((ns_consumed))
#else
#define SWIFT_RELEASES_ARGUMENT
#endif
#if __has_attribute(warn_unused_result)
#define SWIFT_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define SWIFT_WARN_UNUSED_RESULT
#endif
#if __has_attribute(noreturn)
#define SWIFT_NORETURN __attribute__((noreturn))
#else
#define SWIFT_NORETURN
#endif
#if !defined(SWIFT_CLASS_EXTRA)
#define SWIFT_CLASS_EXTRA
#endif
#if !defined(SWIFT_PROTOCOL_EXTRA)
#define SWIFT_PROTOCOL_EXTRA
#endif
#if !defined(SWIFT_ENUM_EXTRA)
#define SWIFT_ENUM_EXTRA
#endif
#if !defined(SWIFT_CLASS)
#if __has_attribute(objc_subclassing_restricted)
#define SWIFT_CLASS(SWIFT_NAME) \
  SWIFT_RUNTIME_NAME(SWIFT_NAME) __attribute__((objc_subclassing_restricted)) SWIFT_CLASS_EXTRA
#define SWIFT_CLASS_NAMED(SWIFT_NAME) \
  __attribute__((objc_subclassing_restricted)) SWIFT_COMPILE_NAME(SWIFT_NAME) SWIFT_CLASS_EXTRA
#else
#define SWIFT_CLASS(SWIFT_NAME) SWIFT_RUNTIME_NAME(SWIFT_NAME) SWIFT_CLASS_EXTRA
#define SWIFT_CLASS_NAMED(SWIFT_NAME) SWIFT_COMPILE_NAME(SWIFT_NAME) SWIFT_CLASS_EXTRA
#endif
#endif
#if !defined(SWIFT_RESILIENT_CLASS)
#if __has_attribute(objc_class_stub)
#define SWIFT_RESILIENT_CLASS(SWIFT_NAME) SWIFT_CLASS(SWIFT_NAME) __attribute__((objc_class_stub))
#define SWIFT_RESILIENT_CLASS_NAMED(SWIFT_NAME) \
  __attribute__((objc_class_stub)) SWIFT_CLASS_NAMED(SWIFT_NAME)
#else
#define SWIFT_RESILIENT_CLASS(SWIFT_NAME) SWIFT_CLASS(SWIFT_NAME)
#define SWIFT_RESILIENT_CLASS_NAMED(SWIFT_NAME) SWIFT_CLASS_NAMED(SWIFT_NAME)
#endif
#endif

#if !defined(SWIFT_PROTOCOL)
#define SWIFT_PROTOCOL(SWIFT_NAME) SWIFT_RUNTIME_NAME(SWIFT_NAME) SWIFT_PROTOCOL_EXTRA
#define SWIFT_PROTOCOL_NAMED(SWIFT_NAME) SWIFT_COMPILE_NAME(SWIFT_NAME) SWIFT_PROTOCOL_EXTRA
#endif

#if !defined(SWIFT_EXTENSION)
#define SWIFT_EXTENSION(M) SWIFT_PASTE(M##_Swift_, __LINE__)
#endif

#if !defined(OBJC_DESIGNATED_INITIALIZER)
#if __has_attribute(objc_designated_initializer)
#define OBJC_DESIGNATED_INITIALIZER __attribute__((objc_designated_initializer))
#else
#define OBJC_DESIGNATED_INITIALIZER
#endif
#endif
#if !defined(SWIFT_ENUM_ATTR)
#if defined(__has_attribute) && __has_attribute(enum_extensibility)
#define SWIFT_ENUM_ATTR(_extensibility) __attribute__((enum_extensibility(_extensibility)))
#else
#define SWIFT_ENUM_ATTR(_extensibility)
#endif
#endif
#if !defined(SWIFT_ENUM)
#define SWIFT_ENUM(_type, _name, _extensibility) \
  enum _name : _type _name;                      \
  enum SWIFT_ENUM_ATTR(_extensibility) SWIFT_ENUM_EXTRA _name : _type
#if __has_feature(generalized_swift_name)
#define SWIFT_ENUM_NAMED(_type, _name, SWIFT_NAME, _extensibility) \
  enum _name : _type _name SWIFT_COMPILE_NAME(SWIFT_NAME);         \
  enum SWIFT_COMPILE_NAME(SWIFT_NAME) SWIFT_ENUM_ATTR(_extensibility) SWIFT_ENUM_EXTRA _name : _type
#else
#define SWIFT_ENUM_NAMED(_type, _name, SWIFT_NAME, _extensibility) \
  SWIFT_ENUM(_type, _name, _extensibility)
#endif
#endif
#if !defined(SWIFT_UNAVAILABLE)
#define SWIFT_UNAVAILABLE __attribute__((unavailable))
#endif
#if !defined(SWIFT_UNAVAILABLE_MSG)
#define SWIFT_UNAVAILABLE_MSG(msg) __attribute__((unavailable(msg)))
#endif
#if !defined(SWIFT_AVAILABILITY)
#define SWIFT_AVAILABILITY(plat, ...) __attribute__((availability(plat, __VA_ARGS__)))
#endif
#if !defined(SWIFT_WEAK_IMPORT)
#define SWIFT_WEAK_IMPORT __attribute__((weak_import))
#endif
#if !defined(SWIFT_DEPRECATED)
#define SWIFT_DEPRECATED __attribute__((deprecated))
#endif
#if !defined(SWIFT_DEPRECATED_MSG)
#define SWIFT_DEPRECATED_MSG(...) __attribute__((deprecated(__VA_ARGS__)))
#endif
#if __has_feature(attribute_diagnose_if_objc)
#define SWIFT_DEPRECATED_OBJC(Msg) __attribute__((diagnose_if(1, Msg, "warning")))
#else
#define SWIFT_DEPRECATED_OBJC(Msg) SWIFT_DEPRECATED_MSG(Msg)
#endif
#if !defined(IBSegueAction)
#define IBSegueAction
#endif
#if !defined(SWIFT_EXTERN)
#if defined(__cplusplus)
#define SWIFT_EXTERN extern "C"
#else
#define SWIFT_EXTERN extern
#endif
#endif
#if __has_feature(modules)
#if __has_warning("-Watimport-in-framework-header")
#pragma clang diagnostic ignored "-Watimport-in-framework-header"
#endif
@import Foundation;
@import ObjectiveC;
#endif

#pragma clang diagnostic ignored "-Wproperty-attribute-mismatch"
#pragma clang diagnostic ignored "-Wduplicate-method-arg"
#if __has_warning("-Wpragma-clang-attribute")
#pragma clang diagnostic ignored "-Wpragma-clang-attribute"
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wnullability"

#if __has_attribute(external_source_symbol)
#pragma push_macro("any")
#undef any
#pragma clang attribute push(                                                                   \
    __attribute__((external_source_symbol(language = "Swift", defined_in = "FirebaseFunctions", \
                                          generated_declaration))),                             \
    apply_to = any(function, enum, objc_interface, objc_category, objc_protocol))
#pragma pop_macro("any")
#endif

@class FIRApp;
@class NSString;
@class FIRHTTPSCallable;
@class NSURL;

/// <code>Functions</code> is the client for Cloud Functions for a Firebase project.
SWIFT_CLASS_NAMED("Functions")
@interface FIRFunctions : NSObject
/// Creates a Cloud Functions client using the default or returns a pre-existing instance if it
/// already exists.
///
/// returns:
/// A shared Functions instance initialized with the default <code>FirebaseApp</code>.
+ (FIRFunctions* _Nonnull)functions SWIFT_WARN_UNUSED_RESULT;
/// Creates a Cloud Functions client with the given app, or returns a pre-existing
/// instance if one already exists.
/// \param app The app for the Firebase project.
///
///
/// returns:
/// A shared Functions instance initialized with the specified <code>FirebaseApp</code>.
+ (FIRFunctions* _Nonnull)functionsForApp:(FIRApp* _Nonnull)app SWIFT_WARN_UNUSED_RESULT;
/// Creates a Cloud Functions client with the default app and given region.
/// \param region The region for the HTTP trigger, such as <code>us-central1</code>.
///
///
/// returns:
/// A shared Functions instance initialized with the default <code>FirebaseApp</code> and a custom
/// region.
+ (FIRFunctions* _Nonnull)functionsForRegion:(NSString* _Nonnull)region SWIFT_WARN_UNUSED_RESULT;
/// Creates a Cloud Functions client with the given app and region, or returns a pre-existing
/// instance if one already exists.
/// \param customDomain A custom domain for the HTTP trigger, such as “https //mydomain.com”.
///
///
/// returns:
/// A shared Functions instance initialized with the default <code>FirebaseApp</code> and a custom
/// HTTP trigger domain.
+ (FIRFunctions* _Nonnull)functionsForCustomDomain:(NSString* _Nonnull)customDomain
    SWIFT_WARN_UNUSED_RESULT;
/// Creates a Cloud Functions client with the given app and region, or returns a pre-existing
/// instance if one already exists.
/// \param app The app for the Firebase project.
///
/// \param region The region for the HTTP trigger, such as <code>us-central1</code>.
///
///
/// returns:
/// An instance of <code>Functions</code> with a custom app and region.
+ (FIRFunctions* _Nonnull)functionsForApp:(FIRApp* _Nonnull)app
                                   region:(NSString* _Nonnull)region SWIFT_WARN_UNUSED_RESULT;
/// Creates a Cloud Functions client with the given app and region, or returns a pre-existing
/// instance if one already exists.
/// \param app The app for the Firebase project.
///
/// \param customDomain A custom domain for the HTTP trigger, such as
/// <code>https://mydomain.com</code>.
///
///
/// returns:
/// An instance of <code>Functions</code> with a custom app and HTTP trigger domain.
+ (FIRFunctions* _Nonnull)functionsForApp:(FIRApp* _Nonnull)app
                             customDomain:(NSString* _Nonnull)customDomain SWIFT_WARN_UNUSED_RESULT;
/// Creates a reference to the Callable HTTPS trigger with the given name.
/// \param name The name of the Callable HTTPS trigger.
///
- (FIRHTTPSCallable* _Nonnull)HTTPSCallableWithName:(NSString* _Nonnull)name
    SWIFT_WARN_UNUSED_RESULT;
- (FIRHTTPSCallable* _Nonnull)HTTPSCallableWithURL:(NSURL* _Nonnull)url SWIFT_WARN_UNUSED_RESULT;
/// Changes this instance to point to a Cloud Functions emulator running locally.
/// See https://firebase.google.com/docs/functions/local-emulator
/// \param host The host of the local emulator, such as “localhost”.
///
/// \param port The port of the local emulator, for example 5005.
///
- (void)useEmulatorWithHost:(NSString* _Nonnull)host port:(NSInteger)port;
- (nonnull instancetype)init SWIFT_UNAVAILABLE;
+ (nonnull instancetype)new SWIFT_UNAVAILABLE_MSG("-init is unavailable");
@end

/// The set of error status codes that can be returned from a Callable HTTPS tigger. These are the
/// canonical error codes for Google APIs, as documented here:
/// https://github.com/googleapis/googleapis/blob/master/google/rpc/code.proto#L26
typedef SWIFT_ENUM_NAMED(NSInteger, FIRFunctionsErrorCode, "FunctionsErrorCode", open){
    /// The operation completed successfully.
    FIRFunctionsErrorCodeOK = 0,
    /// The operation was cancelled (typically by the caller).
    FIRFunctionsErrorCodeCancelled = 1,
    /// Unknown error or an error from a different error domain.
    FIRFunctionsErrorCodeUnknown = 2,
    /// Client specified an invalid argument. Note that this differs from
    /// <code>FailedPrecondition</code>.
    /// <code>InvalidArgument</code> indicates arguments that are problematic regardless of the
    /// state of the
    /// system (e.g., an invalid field name).
    FIRFunctionsErrorCodeInvalidArgument = 3,
    /// Deadline expired before operation could complete. For operations that change the state of
    /// the
    /// system, this error may be returned even if the operation has completed successfully. For
    /// example, a successful response from a server could have been delayed long enough for the
    /// deadline to expire.
    FIRFunctionsErrorCodeDeadlineExceeded = 4,
    /// Some requested document was not found.
    FIRFunctionsErrorCodeNotFound = 5,
    /// Some document that we attempted to create already exists.
    FIRFunctionsErrorCodeAlreadyExists = 6,
    /// The caller does not have permission to execute the specified operation.
    FIRFunctionsErrorCodePermissionDenied = 7,
    /// Some resource has been exhausted, perhaps a per-user quota, or perhaps the entire file
    /// system
    /// is out of space.
    FIRFunctionsErrorCodeResourceExhausted = 8,
    /// Operation was rejected because the system is not in a state required for the operation’s
    /// execution.
    FIRFunctionsErrorCodeFailedPrecondition = 9,
    /// The operation was aborted, typically due to a concurrency issue like transaction aborts,
    /// etc.
    FIRFunctionsErrorCodeAborted = 10,
    /// Operation was attempted past the valid range.
    FIRFunctionsErrorCodeOutOfRange = 11,
    /// Operation is not implemented or not supported/enabled.
    FIRFunctionsErrorCodeUnimplemented = 12,
    /// Internal errors. Means some invariant expected by underlying system has been broken. If you
    /// see one of these errors, something is very broken.
    FIRFunctionsErrorCodeInternal = 13,
    /// The service is currently unavailable. This is a most likely a transient condition and may be
    /// corrected by retrying with a backoff.
    FIRFunctionsErrorCodeUnavailable = 14,
    /// Unrecoverable data loss or corruption.
    FIRFunctionsErrorCodeDataLoss = 15,
    /// The request does not have valid authentication credentials for the operation.
    FIRFunctionsErrorCodeUnauthenticated = 16,
};

@class FIRHTTPSCallableResult;

/// A <code>HTTPSCallable</code> is a reference to a particular Callable HTTPS trigger in Cloud
/// Functions.
SWIFT_CLASS_NAMED("HTTPSCallable")
@interface FIRHTTPSCallable : NSObject
/// The timeout to use when calling the function. Defaults to 70 seconds.
@property(nonatomic) NSTimeInterval timeoutInterval;
/// Executes this Callable HTTPS trigger asynchronously.
/// The data passed into the trigger can be any of the following types:
/// <ul>
///   <li>
///     <code>nil</code> or <code>NSNull</code>
///   </li>
///   <li>
///     <code>String</code>
///   </li>
///   <li>
///     <code>NSNumber</code>, or any Swift numeric type bridgeable to <code>NSNumber</code>
///   </li>
///   <li>
///     <code>[Any]</code>, where the contained objects are also one of these types.
///   </li>
///   <li>
///     <code>[String: Any]</code> where the values are also one of these types.
///   </li>
/// </ul>
/// The request to the Cloud Functions backend made by this method automatically includes a
/// Firebase Installations ID token to identify the app instance. If a user is logged in with
/// Firebase Auth, an auth ID token for the user is also automatically included.
/// Firebase Cloud Messaging sends data to the Firebase backend periodically to collect information
/// regarding the app instance. To stop this, see <code>Messaging.deleteData()</code>. It
/// resumes with a new FCM Token the next time you call this method.
/// \param data Parameters to pass to the trigger.
///
/// \param completion The block to call when the HTTPS request has completed.
///
- (void)callWithObject:(id _Nullable)data
            completion:
                (void (^_Nonnull)(FIRHTTPSCallableResult* _Nullable, NSError* _Nullable))completion;
/// Executes this Callable HTTPS trigger asynchronously. This API should only be used from
/// Objective-C. The request to the Cloud Functions backend made by this method automatically
/// includes a Firebase Installations ID token to identify the app instance. If a user is logged in
/// with Firebase Auth, an auth ID token for the user is also automatically included. Firebase Cloud
/// Messaging sends data to the Firebase backend periodically to collect information regarding the
/// app instance. To stop this, see <code>Messaging.deleteData()</code>. It resumes with a new FCM
/// Token the next time you call this method. \param completion The block to call when the HTTPS
/// request has completed.
///
- (void)callWithCompletion:(void (^_Nonnull)(FIRHTTPSCallableResult* _Nullable,
                                             NSError* _Nullable))completion;
- (nonnull instancetype)init SWIFT_UNAVAILABLE;
+ (nonnull instancetype)new SWIFT_UNAVAILABLE_MSG("-init is unavailable");
@end

/// A <code>HTTPSCallableResult</code> contains the result of calling a <code>HTTPSCallable</code>.
SWIFT_CLASS_NAMED("HTTPSCallableResult")
@interface FIRHTTPSCallableResult : NSObject
/// The data that was returned from the Callable HTTPS trigger.
/// The data is in the form of native objects. For example, if your trigger returned an
/// array, this object would be an <code>Array<Any></code>. If your trigger returned a JavaScript
/// object with keys and values, this object would be an instance of <code>[String: Any]</code>.
@property(nonatomic, readonly) id _Nonnull data;
- (nonnull instancetype)init SWIFT_UNAVAILABLE;
+ (nonnull instancetype)new SWIFT_UNAVAILABLE_MSG("-init is unavailable");
@end

#if __has_attribute(external_source_symbol)
#pragma clang attribute pop
#endif
#pragma clang diagnostic pop
#endif
