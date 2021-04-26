# Firebase C++ SDK

The Firebase C++ SDK provides C++ interfaces for the following Firebase services
on *iOS* and *Android*:

*   AdMob
*   Firebase Analytics
*   Firebase Authentication
*   Firebase Cloud Messaging
*   Firebase Dynamic Links
*   Cloud Firestore
*   Firebase Functions
*   Firebase Installations
*   Firebase Instance ID (deprecated SDK)
*   Firebase Realtime Database
*   Firebase Remote Config
*   Firebase Storage

## Desktop Workflow Implementations

The Firebase C++ SDK includes desktop workflow support for the following subset
of Firebase features, enabling their use on Windows, OS X, and Linux:

*   Firebase Authentication
*   Cloud Firestore
*   Firebase Functions
*   Firebase Remote Config
*   Firebase Realtime Database
*   Firebase Storage

This is a Beta feature, and is intended for workflow use only during the
development of your app, not for publicly shipping code.

## Stub Implementations

Stub (non-functional) implementations of the remaining libraries are provided
for convenience when building for Windows, Mac OS, and Linux so that you don't
need to conditionally compile code when also targeting the desktop.

## Directory Structure

The following table provides an overview of the Firebase C++ SDK directory
structure.

Directories               | Contents
------------------------- | --------
include                   | C++ headers
frameworks/ios/ARCH       | iOS frameworks (compiled against libc++)
                          | A multi-architecture framework is
                          | provided in the *universal* directory.
libs/ios/ARCH             | iOS static libraries (compiled against
                          | libc++)
                          | Multi-architecture libraries are
                          | provided in the *universal* directory.
libs/android/ARCH/STL     | Android (GCC 4.8+ compatible) static
                          | libraries for each architecture and STL
                          | variant.
                          | _STL variants available:_
                          | * `c++`: LLVM libc++ runtime (recommended)
                          | * `gnustl`: GNU STL
                          | * `stlport`: STLport runtime
                          | More information can be found in the
                          | [NDK C++ Helper Runtimes](https://developer.android.com/ndk/guides/cpp-support.html#runtimes)
                          | documentation.
*Desktop Implementations* |
libs/darwin               | OS X static libraries (desktop or stub
                          | implementations, compiled against libc++)
frameworks/darwin         | OS X frameworks (desktop or stub
                          | implementations, compiled against libc++)
libs/linux/ARCH/ABI       | Linux static libraries (desktop or stub
                          | implementations, GCC 4.8+, libc++).
                          | Built against C++11 or legacy ABI.
libs/windows              | Windows static libraries (desktop or stub
                          | implementations, MSVC 2019+)

## Library / Framework Dependencies

Each feature has dependencies upon libraries in this SDK and components
distributed as part of the core Firebase
[iOS SDK](https://firebase.google.com/docs/ios/setup) and
[Android SDK](https://firebase.google.com/docs/android/setup).

### Android Dependencies

Feature                    | Required Libraries and Gradle Packages
-------------------------- | --------------------------------------
Firebase AdMob             | libfirebase_admob.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-ads:20.0.0
                           | (Maven package)
                           | com.google.android.gms:play-services-measurement-sdk-api:18.0.3
                           | (Maven package)
Firebase Analytics         | libfirebase_analytics.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
Firebase Authentication    | libfirebase_auth.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-auth:20.0.4
                           | (Maven package)
Firebase Dynamic Links     | libfirebase_dynamic_links.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-dynamic-links:19.1.1
                           | (Maven package)
Cloud Firestore            | libfirebase_firestore.a
                           | libfirebase_auth.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-firestore:22.1.2
                           | (Maven package)
                           | com.google.firebase:firebase-auth:20.0.4
                           | (Maven package)
Firebase Functions         | libfirebase_functions
                           | libfirebase_auth.a (optional)
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-functions:19.2.0
                           | (Maven package)
                           | com.google.firebase:firebase-auth:20.0.4
                           | (Maven package)
Firebase Installations     | libfirebase_installations.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-installations:16.3.5
                           | (Maven package)
Firebase Instance ID       | libfirebase_instance_id.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-iid:21.1.0
                           | (Maven package)
Firebase Messaging         | libfirebase_messaging.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-messaging:21.1.0
                           | (Maven package)
                           | libmessaging_java.jar (Android service)
                           | androidx.core:core:1.0.1 (Maven package)
Firebase Realtime Database | libfirebase_database.a
                           | libfirebase_auth.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-database:19.7.0
                           | (Maven package)
                           | com.google.firebase:firebase-auth:20.0.4
                           | (Maven package)
Firebase Remote Config     | libfirebase_remote_config.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-config:20.0.4
                           | (Maven package)
Firebase Storage           | libfirebase_storage.a
                           | libfirebase_auth.a
                           | libfirebase_app.a
                           | com.google.firebase:firebase-analytics:18.0.3
                           | (Maven package)
                           | com.google.firebase:firebase-storage:19.2.2
                           | (Maven package)
                           | com.google.firebase:firebase-auth:20.0.4
                           | (Maven package)

Important: Each version of the Firebase C++ SDK supports specific Firebase
Android SDK module versions. Please ensure that you reference the correct
version of each Maven package listed above in your Gradle file.

### iOS Dependencies

iOS users can include either frameworks or static libraries depending upon their
preferred build environment.

#### Frameworks

Feature                    | Required Frameworks and Cocoapods
-------------------------- | ---------------------------------------
Firebase AdMob             | firebase_admob.framework
                           | firebase.framework
                           | Firebase/AdMob Cocoapod (7.11.0)
Firebase Analytics         | firebase_analytics.framework
                           | firebase.framework
                           | Firebase/Analytics Cocoapod (7.11.0)
Firebase Authentication    | firebase_auth.framework
                           | firebase.framework
                           | Firebase/Auth Cocoapod (7.11.0)
Firebase Dynamic Links     | firebase_dynamic_links.framework
                           | firebase.framework
                           | Firebase/DynamicLinks Cocoapod (7.11.0)
Cloud Firestore            | firebase_firestore.framework
                           | firebase_auth.framework
                           | firebase.framework
                           | Firebase/Firestore Cocoapod (7.11.0)
                           | Firebase/Auth Cocoapod (7.11.0)
Firebase Functions         | firebase_functions.framework
                           | firebase_auth.framework (optional)
                           | firebase.framework
                           | Firebase/Functions Cocoapod (7.11.0)
                           | Firebase/Auth Cocoapod (7.11.0)
Firebase Installations     | firebase_installations.framework
                           | firebase.framework
                           | FirebaseInstallations Cocoapod (7.11.0)
Firebase Instance ID       | firebase_instance_id.framework
                           | firebase.framework
                           | FirebaseInstanceID Cocoapod (7.11.0)
Firebase Cloud Messaging   | firebase_messaging.framework
                           | firebase.framework
                           | Firebase/Messaging Cocoapod (7.11.0)
Firebase Realtime Database | firebase_database.framework
                           | firebase_auth.framework
                           | firebase.framework
                           | Firebase/Database Cocoapod (7.11.0)
                           | Firebase/Auth Cocoapod (7.11.0)
Firebase Remote Config     | firebase_remote_config.framework
                           | firebase.framework
                           | Firebase/RemoteConfig Cocoapod (7.11.0)
Firebase Storage           | firebase_storage.framework
                           | firebase_auth.framework
                           | firebase.framework
                           | Firebase/Storage Cocoapod (7.11.0)
                           | Firebase/Auth Cocoapod (7.11.0)

Important: Each version of the Firebase C++ SDK supports a specific version of
the Firebase iOS SDK. Please ensure that you reference the Cocoapod versions
listed above.

#### Libraries

If you prefer to link against static libraries instead of frameworks (see the
previous section) the following table describes the libraries and Cocoapods
required for each SDK feature.

Feature                    | Required Libraries and Cocoapods
-------------------------- | -----------------------------------------
Firebase AdMob             | libfirebase_admob.a
                           | libfirebase_app.a
                           | Firebase/AdMob Cocoapod (7.11.0)
Firebase Analytics         | libfirebase_analytics.a
                           | libfirebase_app.a
                           | Firebase/Analytics Cocoapod (7.11.0)
Firebase Authentication    | libfirebase_auth.a
                           | libfirebase_app.a
                           | Firebase/Auth Cocoapod (7.11.0)
Firebase Dynamic Links     | libfirebase_dynamic_links.a
                           | libfirebase_app.a
                           | Firebase/DynamicLinks Cocoapod (7.11.0)
Cloud Firestore            | libfirebase_firestore.a
                           | libfirebase_app.a
                           | libfirebase_auth.a
                           | Firebase/Firestore Cocoapod (7.11.0)
                           | Firebase/Auth Cocoapod (7.11.0)
Firebase Functions         | libfirebase_functions.a
                           | libfirebase_app.a
                           | libfirebase_auth.a (optional)
                           | Firebase/Functions Cocoapod (7.11.0)
                           | Firebase/Auth Cocoapod (7.11.0)
Firebase Installations     | libfirebase_installations.a
                           | libfirebase_app.a
                           | FirebaseInstallations Cocoapod (7.11.0)
Firebase Instance ID       | libfirebase_instance_id.a
                           | libfirebase_app.a
                           | FirebaseInstanceID Cocoapod (7.11.0)
Firebase Cloud Messaging   | libfirebase_messaging.a
                           | libfirebase_app.a
                           | Firebase/CloudMessaging Cocoapod (7.11.0)
Firebase Realtime Database | libfirebase_database.a
                           | libfirebase_app.a
                           | libfirebase_auth.a
                           | Firebase/Database Cocoapod (7.11.0)
                           | Firebase/Auth Cocoapod (7.11.0)
Firebase Remote Config     | libfirebase_remote_config.a
                           | libfirebase_app.a
                           | Firebase/RemoteConfig Cocoapod (7.11.0)
Firebase Storage           | libfirebase_storage.a
                           | libfirebase_app.a
                           | libfirebase_auth.a
                           | Firebase/Storage Cocoapod (7.11.0)
                           | Firebase/Auth Cocoapod (7.11.0)

Important: Each version of the Firebase C++ SDK supports a specific version of
the Firebase iOS SDK. Please ensure that you reference the Cocoapod versions
listed above.

### Desktop Implementation Dependencies

#### Linux libraries

For Linux, library versions are provided for 32-bit (i386) and 64-bit (x86_64)
platforms.

Two sets of Linux libraries are available: one set built against the newer C++11
ABI, and another set built against the standard (legacy) ABI. This is equivalent
to the compiler option -D_GLIBCXX_USE_CXX11_ABI=1 or 0, respectively.

Feature                         | Required Libraries
------------------------------- | -----------------------------
Firebase Authentication         | libfirebase_auth.a
                                | libfirebase_app.a
Cloud Firestore                 | libfirebase_firestore.a
                                | libfirebase_auth.a
                                | libfirebase_app.a
Firebase Functions              | libfirebase_functions.a
                                | libfirebase_auth.a (optional)
                                | libfirebase_app.a
Firebase Realtime Database      | libfirebase_database.a
                                | libfirebase_auth.a
                                | libfirebase_app.a
Firebase Remote Config          | libfirebase_remote_config.a
                                | libfirebase_app.a
Firebase Storage                | libfirebase_storage.a
                                | libfirebase_auth.a
                                | libfirebase_app.a
Firebase AdMob (stub)           | libfirebase_admob.a
                                | libfirebase_app.a
Firebase Analytics (stub)       | libfirebase_analytics.a
                                | libfirebase_app.a
Firebase Dynamic Links (stub)   | libfirebase_dynamic_links.a
                                | libfirebase_app.a
Firebase Installations (stub)   | libfirebase_installations.a
                                | libfirebase_app.a
Firebase Instance ID (stub)     | libfirebase_instance_id.a
                                | libfirebase_app.a
Firebase Cloud Messaging (stub) | libfirebase_messaging.a
                                | libfirebase_app.a

The provided libraries have been tested using GCC 4.8.0, GCC 7.2.0, and Clang
5.0 on Ubuntu. When building C++ desktop apps on Linux, you will need to link
the `pthread` system library (consult your compiler documentation for more
information).

#### OS X libraries

For OS X (Darwin), library versions are only provided for the 64-bit (x86_64)
platform. See the table above (in the "Linux libraries" section) for the list of
library dependencies. XCFrameworks are also provided for convenience.

Feature                         | Required XCFrameworks
------------------------------- | ----------------------------------
Firebase Authentication         | firebase_auth.xcframework
                                | firebase.xcframework
Cloud Firestore                 | firebase_firestore.xcframework
                                | firebase_auth.xcframework
                                | firebase.xcframework
Firebase Functions              | firebase_functions.xcframework
                                | firebase_auth.xcframework (optional)
                                | firebase.xcframework
Firebase Realtime Database      | firebase_database.xcframework
                                | firebase_auth.xcframework
                                | firebase.xcframework
Firebase Remote Config          | firebase_remote_config.xcframework
                                | firebase.xcframework
Firebase Storage                | firebase_storage.xcframework
                                | firebase_auth.xcframework
                                | firebase.xcframework
Firebase AdMob (stub)           | firebase_admob.xcframework
                                | firebase.xcframework
Firebase Analytics (stub)       | firebase_analytics.xcframework
                                | firebase.xcframework
Firebase Dynamic Links (stub)   | firebase_dynamic_links.xcframework
                                | firebase.xcframework
Firebase Installations (stub)   | firebase_installations.xcframework
                                | firebase.xcframework
Firebase Instance ID (stub)     | firebase_instance_id.xcframework
                                | firebase.xcframework
Firebase Cloud Messaging (stub) | firebase_messaging.xcframework
                                | firebase.xcframework

The provided libraries have been tested using Xcode 12.0. When building C++
desktop apps on OS X, you will need to link the `gssapi_krb5` and `pthread`
system libraries, as well as the `CoreFoundation`, `Foundation`, `GSS`, and
`Security` OS X system frameworks (consult your compiler documentation for more
information).

#### Windows libraries

For Windows, library versions are provided depending on whether your project is
building in 32-bit (x86) or 64-bit (x64) mode, which Windows runtime environment
you are using (Multithreaded /MT or Multithreaded DLL /MD), and whether you are
targeting Release or Debug.

Feature                         | Required Libraries and Gradle Packages
------------------------------- | --------------------------------------
Firebase Authentication         | firebase_auth.lib
                                | firebase_app.lib
Cloud Firestore                 | firebase_firestore.lib
                                | firebase_auth.lib
                                | firebase_app.lib
Firebase Functions              | firebase_functions.lib
                                | firebase_auth.lib (optional)
                                | firebase_app.lib
Firebase Realtime Database      | firebase_database.lib
                                | firebase_auth.lib
                                | firebase_app.lib
Firebase Remote Config          | firebase_remote_config.lib
                                | firebase_app.lib
Firebase Storage                | firebase_storage.lib
                                | firebase_auth.lib
                                | firebase_app.lib
Firebase AdMob (stub)           | firebase_admob.lib
                                | firebase_app.lib
Firebase Analytics (stub)       | firebase_analytics.lib
                                | firebase_app.lib
Firebase Dynamic Links (stub)   | firebase_dynamic_links.lib
                                | firebase_app.lib
Firebase Installations (stub)   | firebase_installations.lib
                                | firebase_app.lib
Firebase Instance ID (stub)     | firebase_instance_id.lib
                                | firebase_app.lib
Firebase Cloud Messaging (stub) | firebase_messaging.lib
                                | firebase_app.lib

The provided libraries have been tested using Visual Studio 2019. When
building C++ desktop apps on Windows, you will need to link the following
Windows SDK libraries (consult your compiler documentation for more
information):

Firebase C++ Library | Windows SDK library dependencies
-------------------- | -----------------------------------------------------
Authentication       | `advapi32, ws2_32, crypt32`
Firestore            | `advapi32, ws2_32, crypt32, rpcrt4, ole32, shell32`
Functions            | `advapi32, ws2_32, crypt32, rpcrt4, ole32`
Realtime Database    | `advapi32, ws2_32, crypt32, iphlpapi, psapi, userenv, shell32`
Remote Config        | `advapi32, ws2_32, crypt32, rpcrt4, ole32`
Storage              | `advapi32, ws2_32, crypt32`

## Getting Started

See our [setup guide](https://firebase.google.com/docs/cpp/setup) to get started
and download the prebuilt version of the Firebase C++ SDK.

## Source Code

The Firebase C++ SDK is open source. You can find the source code (and
information about building it) at
[github.com/firebase/firebase-cpp-sdk](https://github.com/firebase/firebase-cpp-sdk).

## Platform Notes

### iOS Method Swizzling

On iOS, some application events (such as opening URLs and receiving
notifications) require your application delegate to implement specific methods.
For example, receiving a notification may require your application delegate to
implement `application:didReceiveRemoteNotification:`. Because each iOS
application has its own app delegate, Firebase uses _method swizzling_, which
allows the replacement of one method with another, to attach its own handlers in
addition to any you may have implemented.

The Firebase Cloud Messaging library needs to attach
handlers to the application delegate using method swizzling. If you are using
these libraries, at load time, Firebase will identify your `AppDelegate` class
and swizzle the required methods onto it, chaining a call back to your existing
method implementation.

### Custom Android Build Systems

We currently provide generate\_xml\_from\_google\_services\_json.py to convert
google-services.json to .xml resources to be included in an Android application.
This script applies the same transformation that the Google Play Services Gradle
plug-in performs when building Android applications. Users who don't use Gradle
(e.g ndk-build, makefiles, Visual Studio etc.) can use this script to automate
the generation of string resources.

### ProGuard on Android

Many Android build systems use
[ProGuard](https://developer.android.com/studio/build/shrink-code.html) for
builds in Release mode to shrink application sizes and protect Java source code.
If you use ProGuard, you will need to add the files in libs/android/*.pro
corresponding to the Firebase C++ libraries you are using to your ProGuard
configuration.

For example, with Gradle, build.gradle would contain:
~~~
  android {
    [...other stuff...]
    buildTypes {
      release {
        minifyEnabled true
        proguardFile getDefaultProguardFile('your-project-proguard-config.txt')
        proguardFile file(project.ext.firebase_cpp_sdk_dir + "/libs/android/app.pro")
        proguardFile file(project.ext.firebase_cpp_sdk_dir + "/libs/android/analytics.pro")
        [...and so on, for each Firebase C++ library you are using.]
      }
    }
  }
~~~

### Requiring Google Play services on Android

Many Firebase C++ libraries require
[Google Play services](https://developers.google.com/android/guides/overview) on
the user's Android device. If a Firebase C++ library returns
[`kInitResultFailedMissingDependency`](http://firebase.google.com/docs/reference/cpp/namespace/firebase)
on initialization, it means Google Play services is not available on the device
(it needs to be updated, reactivated, permissions fixed, etc.) and that Firebase
library cannot be used until the situation is corrected.

You can find out why Google Play services is unavailable (and try to fix it) by
using the functions in
[`google_play_services/availability.h`](http://firebase.google.com/docs/reference/cpp/namespace/google-play-services).

Optionally, you can use
[`ModuleInitializer`](http://firebase.google.com/docs/reference/cpp/class/firebase/module-initializer)
to initialize one or more Firebase libraries, which will handle prompting the
user to update Google Play services if required.

Note: Some libraries do not require Google Play services and don't return any
initialization status. These can be used without Google Play services. The table
below summarizes whether Google Play services is required by each Firebase C++
library.

Firebase C++ Library | Google Play services required?
-------------------- | ---------------------------------
Analytics            | Not required
AdMob                | Not required (usually; see below)
Cloud Messaging      | Required
Auth                 | Required
Dynamic Links        | Required
Firestore            | Required
Functions            | Required
Installations        | Not Required
Instance ID          | Required
Realtime Database    | Required
Remote Config        | Required
Storage              | Required

#### A note on AdMob and Google Play services

Most versions of the Google Mobile Ads SDK for Android can work properly without
Google Play services. However, if you are using the
`com.google.android.gms:play-services-ads-lite` dependency instead of the
standard `com.google.firebase:firebase-ads` dependency, Google Play services
WILL be required in your specific case.

AdMob initialization will only return `kInitResultFailedMissingDependency` when
Google Play services is unavailable AND you are using
`com.google.android.gms:play-services-ads-lite`.

### Desktop project setup

To use desktop workflow support, you must have an Android or iOS project set up
in the Firebase console.

If you have an Android project, you can simply use the `google-services.json`
file on desktop.

If you have an iOS project and don't wish to create an Android project, you can
use the included Python script `generate_xml_from_google_services_json.py
--plist` to convert your `GoogleService-Info.plist` file into a
`google-services-desktop.json` file.

By default, when your app initializes, Firebase will look for a file named
`google-services.json` or `google-services-desktop.json` in the current
directory. Ensure that one of these files is present, or call
`AppOptions::LoadFromJsonConfig()` before initializing Firebase to specify your
JSON configuration data directly.

### Note on Firebase C++ desktop support

Firebase C++ SDK desktop support is a **Beta** feature, and is intended for
workflow use only during the development of your app, not for publicly shipping
code.

## Release Notes

### 7.3.0
-   Changes
    -   General (iOS): Update dependencies.
    -   General (Android): Fix a gradle error if ANDROID_NDK_HOME is not set.

### 7.2.0
-   Changes
    -   General (Android): Firebase support for STLPort is deprecated and will
        be removed in the next major release. Please
        [use libc++ instead](https://developer.android.com/ndk/guides/cpp-support#cs).
    -   General (iOS): iOS SDKs are now built using Xcode 12.
    -   General (iOS): iOS SDKs are now providing XCFrameworks instead of
        Frameworks.
    -   Database: Fixed a potential crash that can occur as a result of a race
        condidtion when adding, removing and deleting `ValueListener`s or
        `ChildListener`s rapidly.
    -   Database: Fixed a crash when setting large values on Windows and Mac
        systems ([#517](https://github.com/firebase/quickstart-unity/issues/517)).
    -   General: Fixed rare crashes at application exit when destructors were
        being executed
        ([#345](https://github.com/firebase/firebase-cpp-sdk/pull/345)).
    -   General (Android): Removed checks for Google Play services for Auth, Database,
        Functions and Storage as the native Android packages no longer need it.
        ([#361](https://github.com/firebase/firebase-cpp-sdk/pull/361)).

### 7.1.1
-   Changes
    -   General (Android): Use non-conflicting file names for embedded resources
        in Android builds. This fixes segfault crashes on old Android devices
        (Android 5 and below).

### 7.1.0
-   Changes
    -   General (iOS): Re-enabled Bitcode in iOS builds
        ([#266][https://github.com/firebase/firebase-cpp-sdk/issues/266]).
    -   Auth: You can now specify a language for emails and text messages sent
        from your apps using `UseAppLanguage()` or `set_language_code()`.
    -   Firestore: Fixed partial updates in `Update()` with
        `FieldValue::Delete()`
        ([#882](https://github.com/firebase/quickstart-unity/issues/882)).
    -   Messaging (Android): Now uses `enqueueWork` instead of `startService`.
        This fixes lost messages with data payloads received when the app
        is in the background.
        ([#877](https://github.com/firebase/quickstart-unity/issues/877)
    -   Remote Config: Added `firebase::remote_config::RemoteConfig` class with
        new instance-based APIs to better manage fetching config data.
    -   Remote Config: Deprecated old module-based API in favor of the new
        instance-based API instead.
    -   Remote Config (Desktop): Fixed multiple definition of Nanopb symbols
        in binary SDK
        ([#271][https://github.com/firebase/firebase-cpp-sdk/issues/271]).

### 7.0.1
-   Changes
    -   Installations (Android): Fix incorrect STL variants, which fixes
        a linker error on Android.

### 7.0.0
-   Changes
    -   General (iOS): iOS SDKs are now built using Xcode 11.7.
    -   General (Desktop): Windows libraries are now built using Visual
        Studio 2019. While VS 2019 is binary-compatible with VS 2015 and
        VS 2017, you must use VS 2019 or newer to link the desktop SDK.
        The libraries have been moved from libs/windows/VS2015 to
        libs/windows/VS2019 to reflect this.
    -   General (Desktop): Linux libraries are now built with both the
        C++11 ABI and the legacy ABI. The libraries have been moved
        from libs/linux/${arch} to libs/linux/${arch}/legacy and
        libs/linux/${arch}/cxx11 to reflect this.
    -   AdMob (Android): Fix a JNI error when initializing without Firebase App.
    -   Analytics: Remove deprecated SetMinimumSessionDuration call.
    -   Installations: Added Installations SDK. See [Documentations](http://firebase.google.com/docs/reference/cpp/namespace/firebase/installations) for
        details.
    -   Instance Id: Marked Instance Id as deprecated.
    -   Messaging: Added getToken, deleteToken apis.
    -   Messaging: Removed deprecated Send() function.
    -   Messaging: raw_data has been changed from a std::string to a
        std::vector<uint8_t>, and can now be populated.
    -   Firestore: Added support for `Query::WhereNotEqualTo` and
        `Query::WhereNotIn`.
    -   Firestore: Added support for `Settings::set_cache_size_bytes` and
        `Settings::cache_size_bytes`.
    -   Firestore: `Query` methods that return new `Query` objects are now
        `const`.
    -   Firestore: Added new internal HTTP headers to the gRPC connection.
    -   Firestore: Fixed a crash when writing to a document after having been
        offline for long enough that the auth token expired
        ([#182](https://github.com/firebase/firebase-cpp-sdk/issues/182)).

### 6.16.1
-   Changes
    -   Database (Desktop): Added a function to create directories recursively
        for persistent storage that fixes segfaults.
    -   Database (Desktop): Fixed a problem with missing symbols on Windows.

### 6.16.0
-   Changes
    -   Database (Desktop): Enabled offline persistence.
    -   Auth: Fixed compiler error related to SignInResult.
    -   Firestore: Defaulted to calling listeners and other callbacks on a
        dedicated thread. This avoids deadlock when using Firestore without
        an event loop on desktop platforms.
    -   Firestore: Added `Error::kErrorNone` as a synonym for `Error::kErrorOk`,
        which is more consistent with other Firebase C++ SDKs.
    -   Messaging (Android): Updated library to be compatible with Android O,
        which should resolve a IllegalStateException that could occur under
        certain conditions.
    -   Messaging: Deprecated the `Send` function.
    -   Firestore: Added `error_message` parameter to snapshot listener
        callbacks.
    -   AdMob: Handling IllegalStateException when creating and loading
        interstitial ads. Added `ConstantsHelper.CALLBACK_ERROR_UNKNOWN` as a
        fallback error.

### 6.15.1

-   Changes

    -   Firestore: all members of `Error` enumeration are now prefixed with
        `kError`; for example, `Error::kUnavailable` is now
        `Error::kErrorUnavailable`, which is more consistent with other Firebase
        C++ SDKs.
    -   Firestore: Firestore can now be compiled on Windows even in presence of
        `min` and `max` macros defined in `<windows.h>`.
    -   Fixed an issue that warns about Future handle not released properly.

### 6.15.0

-   Overview
    -   Fixed an issue whent creating Apps, and various Firestore changes.
-   Changes
    -   Firestore: removed `*LastResult` functions from the public API. Please
        use the futures returned by the async methods directly instead.
    -   Firestore: dropped the `From` prefix from the static functions in
        `FieldValue`; for example, `FieldValue::FromInteger` is now just
        `FieldValue::Integer`.
    -   Firestore: `CollectionReference::id` now returns a const reference.
    -   Firestore: Fixed absl `time_internal` linker error on Windows.
    -   Firestore: changed the signature of the callback passed to
        `Firestore::RunTransaction` to pass the parameters by mutable reference,
        not by pointer. This is to indicate that these parameters are never
        null.
    -   App: Fixed an assert creating a custom App when there is no default App.

### 6.14.1

-   Changes
    -   Auth (iOS): Added SignInResult.UserInfo.updated_credential field. On
        iOS, kAuthErrorCredentialAlreadyInUse errors when linking with Apple may
        contain a valid updated_credential for use in signing-in the
        Apple-linked user.

### 6.14.0

-   Changes
    -   Firestore: `Firestore::set_logging_enabled` is replaced by
        `Firestore::set_log_level` for consistency with other Firebase C++ APIs.
    -   Firestore: added an overload of `Firestore::CollectionGroup` that takes
        a pointer to `const char`.
    -   Firestore: `Firestore::set_settings` now accepts the argument by value.

### 6.12.0
  - Overview
    - Added experimental support for Cloud Firestore SDK.
  - Changes
    - Firestore: Experimental release of Firestore is now available on all
      supported platforms.

### 6.11.0

-   Overview
    -   Updated dependencies, changed minimum Xcode, and fixed an issue in
        Database handling Auth token revocation.
-   Changes
    -   General (iOS): Minimum Xcode version is now 10.3.
    -   General: When creating an App, the project_id from the default App is
        used if one is not provided.
    -   Database (Desktop): Fixed that database stops reconnecting to server
        after the auth token is revoked.

### 6.10.0

-   Overview
    -   Auth bug fixes.
-   Changes
    -   Auth: Reverted the API of an experimental FederatedAuthHandler callback
        handler.
    -   Auth (iOS): Enabled the method OAuthProvider.GetCredential. This method
        takes a nonce parameter as required by Apple Sign-in.
    -   General (iOS): Updated the CMakeLists.txt to link static libraries
        stored under libs/ios/universal for iOS targets

### 6.9.0

-   Overview
    -   Updated dependencies, added support for Apple Sign-in to Auth, support
        for signing-in using a 3rd party web providers and configuration of
        BigQuery export in Messaging.
-   Changes
    -   Auth: Added API for invoking Auth SignInWithProvider and User
        LinkWithProvider and ReauthenticateWithProvider for sign in with third
        party auth providers.
    -   Auth: Added constant kProviderId strings to auth provider classes.
    -   Auth (iOS): Added support for linking Apple Sign-in credentials.
    -   Messaging (Android): Added the option to enable or disable message
        delivery metrics export to BigQuery. This functionality is currently
        only available on Android. Stubs are provided on iOS for cross platform
        compatibility.

### 6.8.0

-   Overview
    -   Updated dependencies, added compiler/stdlib check, fixed issue in Admob
        and fixed resource generation issue with python3.
-   Changes
    -   App (Linux): Added compiler/stdlib check to ensure both the developer's
        executable and firebase library are compiled with the same compiler and
        stdlib.
    -   Admob (Android): Fixed a potentially non thread safe operation in the
        destruction of BannerViews.
    -   General: Fixed an issue where resource generation from
        google-services.json would fail if python3 was used to execute the
        resource generation script.

### 6.7.0

-   Overview
    -   Updated dependencies, fixed issues in Analytics, Database, Storage, and
        App.
-   Changes
    -   App: Added noexcept to move constructors to ensure STL uses move where
        possible.
    -   Storage (iOS/Android): Fixed an issue where Storage::GetReferenceFromUrl
        would return an invalid StorageReference.
    -   Database: Fixed an issue causing timestamps to not be populated
        correctly when using DatabaseReference::UpdateChildren().
    -   Database (Desktop): Fixed an issue preventing listener events from being
        triggered after DatabaseReference::UpdateChildren() is called.
    -   Database (Desktop): Functions that take const char* parameters will now
        fail gracefully if passed a null pointer.
    -   Database (Desktop): Fixed an issue causing
        DatabaseReference::RunTransaction() to fail due to datastale when the
        location previously stored a vector with more than 10 items or a map
        with integer keys.
    -   App (Windows): Fixed bug where literal value 0 will call string
        constructor for Variant class.
    -   Storage (Desktop): Changed url() to return the empty string if the
        Storage instance was created with the default (null) URL.
    -   App: Added small string optimisation for variant.
    -   App: Reduced number of new/delete for variant if copying same type
    -   App: Ensure map sort order for variant is consistent.
    -   Database (Desktop): Fixed an issue that could result in an incorrect
        snapshot being passed to listeners under specific circumstances.
    -   Analytics (iOS): Fixed the racy behavior of
        `analytics::GetAnalyticsInstanceId()` after calling
        `analytics::ResetAnalyticsData()`.
    -   Database (Desktop): Fixed ordering issue of children when using OrderBy
        on double or int64 types with large values

### 6.6.1

-   Overview
    -   Fixed an issue with Future having an undefined reference.
-   Changes
    -   General: Fixed a potential undefined reference in Future::OnCompletion.

### 6.6.0

-   Overview
    -   Update dependencies, fixed issues in Auth, Database and RemoteConfig
-   Changes
    -   Auth (Android): Fixed assert when not using default app instance.
    -   Auth (Desktop): Fixed not loading provider list from cached user data.
    -   Database (Desktop): Fixed a crash that could occur when trying to keep a
        location in the database synced when you do not have permission.
    -   Database (Desktop): Queries on locations in the database with query
        rules now function properly, instead of always returning "Permission
        denied".
    -   Database (Desktop): Fixed the map-to-vector conversion when firing
        events that have maps containing enitrely integer keys.
    -   Remote Config (Android): Fixed a bug when passing a Variant of type Blob
        to SetDefaults() on Android.

### 6.5.0

-   Overview
    -   Updated dependencies, and improved logging for Auth and Database.
-   Changes
    -   Auth (Linux): Improved error logging if libsecret (required for login
        persistence) is not installed on Linux.
    -   Database: The database now supports setting the log level independently
        of the system level logger.

### 6.4.0

-   Overview
    -   Updated dependencies, fixed issues with Futures and Auth persistence,
        and fixed a crash in Database.
-   Changes
    -   General: Fixed an issue causing Futures to clear their data even if a
        reference was still being held.
    -   Auth (Desktop): Fixed an issue with updated user info not being
        persisted.
    -   Database (Desktop): Fixed a crash when saving a ServerTimestamp during a
        transaction.

### 6.3.0

-   Overview
    -   Bug fixes.
-   Changes
    -   General (iOS/Android): Fixed a bug that allows custom firebase::App
        instances to be created after the app has been restarted.
    -   Auth (desktop): Changed destruction behavior. Instead of waiting for all
        async operations to finish, now Auth will cancel all async operations
        and quit. For callbacks that are already running, this will protect
        against cases where auth instances might not exist anymore.
    -   Auth (iOS): Fixed an exception in firebase::auth::VerifyPhoneNumber.
    -   Auth (iOS): Stopped Auth from hanging on destruction if any local
        futures remain in scope.
    -   Database (desktop): Fixed an issue that could cause a crash when
        updating the descendant of a location with a listener attached.

### 6.2.2

-   Overview
    -   Bug fixes.
-   Changes
    -   Auth (desktop): After loading a persisted user data, ensure token is not
        expired.
    -   Auth (desktop): Ensure Database, Storage and Functions do not use an
        expired token after it's loaded from persistent storage.
    -   Database (desktop): Fixed DatabaseReference::RunTransaction() sending
        invalid data to the server which causes error message "Error on incoming
        message" and freeze.

### 6.2.0

-   Overview
    -   Added support for custom domains to Dynamic Links, and fixed issues in
        Database and Instance ID.
-   Changes
    -   General: Added a way to configure SDK-wide log verbosity.
    -   Instance ID (Android): Fixed a crash when destroying InstanceID objects.
    -   Dynamic Links: Added support for custom domains.
    -   Database: Added a way to configure log verbosity of Realtime Database
        instances.

### 6.1.0

-   Overview
    -   Added Auth credential persistence on Desktop, fixed issues in Auth and
        Database, and added additional information to Messaging notifications.
-   Changes
    -   Auth (Desktop): User's credentials will now persist between sessions.
        See the
        [documentation](http://firebase.google.com/docs/auth/cpp/manage-users#persist_a_users_credential)
        for more information.
    -   Auth (Desktop): As part of the above change, if you call current_user()
        immediately after creating the Auth instance, it will block until the
        saved user's state is finished loading.
    -   Auth (Desktop): Fixed an issue where Database/Functions/Storage might
        not use the latest auth token immediately after sign-in.
    -   Auth: Fixed an issue where an error code could get reported incorrectly
        on Android.
    -   Database (Desktop): Fixed an issue that could cause a crash during
        shutdown.
    -   Database (iOS): Fixed a race condition that could cause a crash when
        cleaning up database listeners on iOS.
    -   Database (iOS): Fixed an issue where long (64-bit) values could get
        written to the database incorrectly (truncated to 32-bits).
    -   Cloud Functions: Change assert to log warning when App is deleted before
        Cloud Functions instance is deleted.
    -   Messaging (Android): Added channel_id to Messaging notifications.

### 6.0.0

-   Overview
    -   Fixed issues in Auth and Messaging, removed Firebase Invites, removed
        deprecated functions in Firebase Remote Config, and deprecated a
        function in Firebase Analytics.
-   Changes
    -   Updated
        [Firebase iOS](https://firebase.google.com/support/release-notes/ios#6.0.0)
        and
        [Firebase Android](https://firebase.google.com/support/release-notes/ios#2019-05-07)
        dependencies.
    -   Auth: Fixed a race condition updating the current user.
    -   Messaging (iOS/Android): Fix an issue where Subscribe and Unsubscribe
        never returned if the API was configured not to receive a registration
        token.
    -   Invites: Removed Firebase Invites library, as it is no longer supported.
    -   Remote Config: Removed functions using config namespaces.
    -   Analytics: Deprecated SetMinimumSessionDuration.
-   Known Issues
    -   To work around a incompatible dependency, AdMob for Android temporarily
        requires an additional dependency on
        com.google.android.gms:play-services-measurement-sdk-api:16.5.0

### 5.7.0

-   Overview
    -   Deprecated functions in Remote Config.
-   Changes
    -   Remote Config: Config namespaces are now deprecated. You'll need to
        switch to methods that use the default namespace.
-   Known Issues
    -   To work around a incompatible dependency, AdMob for Android temporarily
        requires an additional dependency on
        com.google.android.gms:play-services-measurement-sdk-api:16.4.0

### 5.6.1

-   Overview
    -   Fixed race condition on iOS SDK startup.
-   Changes
    -   General (iOS): Updated to the latest iOS SDK to fix a crash on
        firebase::App creation caused by a race condition. The crash could occur
        when accessing the [FIRApp firebaseUserAgent] property of the iOS
        FIRApp.

### 5.6.0

-   Overview
    -   Released an open-source version, added Game Center sign-in to Auth,
        enhanced Database on desktop, and fixed a crash when deleting App.
-   Changes
    -   Firebase C++ is now
        [open source](https://github.com/firebase/firebase-cpp-sdk).
    -   Auth (iOS): Added Game Center authentication.
    -   Database (Desktop): Reworked how cached server values work to be more in
        line with mobile implementations.
    -   Database (Desktop): Simultaneous transactions are now supported.
    -   Database (Desktop): The special Timestamp ServerValue is now supported.
    -   Database (Desktop): KeepSynchronized is now supported.
    -   App, Auth, Database, Remote Config, Storage: Fixed a crash when deleting
        `firebase::App` before deleting other Firebase subsystems.

### 5.5.0

-   Overview
    -   Deprecated Firebase Invites and updated how Android dependencies are
        included.
-   Changes
    -   General (Android): Added a gradle file to the SDK that handles adding
        Firebase Android dependencies to your Firebase C++ apps. See the
        [Firebase C++ Samples](https://github.com/firebase/quickstart-cpp) for
        example usage.
    -   Invites: Firebase Invites is deprecated. Please refer to
        https://firebase.google.com/docs/invites for details.

### 5.4.4

-   Overview
    -   Fixed a bug in Cloud Functions on Android, and AdMob on iOS.
-   Changes
    -   Cloud Functions (Android): Fixed an issue with error handling.
    -   AdMob (iOS): Fixed an issue with Rewarded Video ad unit string going out
        of scope.

### 5.4.3

-   Overview
    -   Bug fix for Firebase Storage on iOS.
-   Changes
    -   Storage (iOS): Fixed an issue when downloading files with `GetBytes`.

### 5.4.2

-   Overview
    -   Removed a spurious error message in Auth on Android.
-   Changes
    -   Auth (Android): Removed an irrelevant error about the Java class
        FirebaseAuthWebException.

### 5.4.0

-   Overview
    -   Bug fix for link shortening in Dynamic Links and a known issue in
        Database on Desktop.
-   Changes
    -   Dynamic Links (Android): Fixed short link generation failing with "error
        8".
-   Known Issues
    -   The Realtime Database Desktop SDK uses REST to access your database.
        Because of this, you must declare the indexes you use with
        Query::OrderByChild() on Desktop or your listeners will fail.

### 5.3.1

-   Overview
    -   Updated iOS and Android dependency versions and a bug fix for Invites.
-   Changes
    -   Invites (Android): Fixed an exception when the Android Minimum Version
        code option is used on the Android.

### 5.3.0

-   Overview
    -   Fixed bugs in Database and Functions; changed minimum Xcode version to
        9.4.1.
-   Changes
    -   General (iOS): Minimum Xcode version is now 9.4.1.
    -   Functions (Android): Fixed an issue when a function returns an array.
    -   Database (Desktop): Fixed issues in ChildListener.
    -   Database (Desktop): Fixed crash that can occur if Database is deleted
        while asynchronous operations are still in progress.
-   Known Issues
    -   Dynamic Links (Android): Shortening dynamic links fails with "Error 8".

### 5.2.1

-   Overview
    -   Fixed bugs in Auth and Desktop.
-   Changes
    -   Database (Desktop): Fixed support for `ChildListener` when used with
        `Query::EqualTo()`, `Query::StartAt()`, `Query::EndAt()`,
        `Query::LimitToFirst()` and `Query::LimitToLast()`.
    -   Database: Fixed a crash in DatabaseReference/Query copy assignment
        operator and copy constructor.
    -   Auth, Database: Fixed a race condition returning Futures when calling
        the same method twice in quick succession.

### 5.2.0

-   Overview
    -   Changes to Database, Functions, Auth, and Messaging.
-   Changes
    -   Database: Added a version of `GetInstance` that allows passing in the
        URL of the database to use.
    -   Functions: Added a way to specify which region to run the function in.
    -   Messaging: Changed `Subscribe` and `Unsubscribe` to return a Future.
    -   Auth (Android): Fixed a crash in `User::UpdatePhoneNumberCredential()`.
    -   General (Android): Fixed a null reference in the Google Play Services
        availability checker.

### 5.1.1

-   Overview
    -   Updated Android and iOS dependency versions only.

### 5.1.0

-   Overview
    -   Changes to Analytics, Auth, and Database; and added support for Cloud
        Functions for Firebase.
-   Changes
    -   Analytics: Added `ResetAnalyticsData()` to clear all analytics data for
        an app from the device.
    -   Analytics: Added `GetAppInstanceId()` which allows developers to
        retrieve the current app's analytics instance ID.
    -   Auth: Linking a credential with a provider that has already been linked
        now produces an error.
    -   Auth (iOS): Fixed crashes in User::LinkAndRetrieveDataWithCredential()
        and User::ReauthenticateAndRetrieveData().
    -   Auth (iOS): Fixed photo URL never returning a value on iOS.
    -   Auth (Android): Fixed setting profile photo URL with UpdateUserProfile.
    -   Database: Added support for ServerValues in SetPriority methods.
    -   Functions: Added support for Cloud Functions for Firebase on iOS,
        Android, and desktop.

### 5.0.0

-   Overview
    -   Renamed the Android and iOS libraries to include firebase in their name,
        removed deprecated methods in App, AdMob, Auth, Database, and Storage,
        and exposed new APIs in Dynamic Links and Invites.
-   Changes
    -   General (Android/iOS): Library names have been prefixed with
        "firebase_", for example libapp.a is now libfirebase_app.a. This brings
        them in line with the naming scheme used on desktop, and iOS frameworks.
    -   General (Android): Improved error handling when device is out of space.
    -   App: Removed deprecated accessor methods from Future.
    -   AdMob: Removed deprecated accessor methods from BannerView and
        InterstitialAd.
    -   Auth: Removed deprecated accessors from Auth, Credential, User, and
        UserInfoInterface, including User::refresh_token().
    -   Database: Removed deprecated accessors from DatabaseReference, Query,
        DataSnapshot, and MutableData.
    -   Dynamic Links: Added a field to received dynamic links describing the
        strength of the match.
    -   Invites: Added OnInviteReceived to Listener base class that includes the
        strength of the match on the received invite as an enum. Deprecated
        prior function that received it as a boolean value.
    -   Storage: Removed deprecated accessors from StorageReference.
    -   Storage: Removed Metadata::download_url() and Metadata::download_urls().
        Please use StorageReference::GetDownloadUrl() instead.
    -   Messaging: Added an optional initialization options struct. This can be
        used to suppress the prompt on iOS that requests permission to receive
        notifications at start up. Permission can be requested manually using
        the function firebase::messaging::RequestPermission().

### 4.5.1

-   Overview
    -   Fixed bugs in Database (Desktop) and Remote Config and exposed new APIs
        in Auth on Desktop and Messaging.
-   Changes
    -   Messaging: Added the SetAutoTokenRegistrationOnInitEnabled() and
        IsAutoTokenRegistrationOnInitEnabled() methods to enable or disable
        auto-token generation.
    -   Auth (Desktop): Added support for accessing user metadata.
    -   Database (Desktop): Fixed a bug to make creation of database instances
        with invalid URLs return NULL.
    -   Database (Desktop): Fixed an issue where incorrect values could be
        passed to OnChildAdded.
    -   Remote Config: Fixed a bug causing incorrect reporting of success or
        failure during a Fetch().

### 4.5.0

-   Overview
    -   Desktop workflow support for some features, Google Play Games
        authentication on Android, and changes to AdMob, Auth, and Storage.
-   Changes
    -   Auth, Realtime Database, Remote Config, Storage (Desktop): Stub
        implementations have been replaced with functional desktop
        implementations on Windows, OS X, and Linux.
    -   AdMob: Native Express ads have been discontinued, so
        `NativeExpressAdView` has been marked deprecated and will be removed in
        a future version.
    -   Auth (Android): Added Google Play Games authentication.
    -   Auth: Fixed a race condition initializing/destroying Auth instances.
    -   Storage: Added MD5 hash to Metadata.
    -   Storage: Fixed a crash when deleting listeners and other object
        instances.
    -   Storage: Controller can now be used from any thread.
    -   Storage (iOS): Fixed incorrect content type when uploading.
-   Known Issues
    -   When using Firebase Realtime Database on desktop, only one Transaction
        may be run on a given subtree at the same time.
    -   When using Firebase Realtime Database on desktop, data persistence is
        not available.

### 4.4.3

-   Overview
    -   Fixed linking bug in App.
-   Changes
    -   App (iOS): Removed unresolved symbols in the App library that could
        cause errors when forcing resolution.

### 4.4.2

-   Overview
    -   Fixed bugs in Dynamic Links, Invites, Remote Config and Storage and
        fixed linking issues with the Windows and Linux stub libraries.
-   Changes
    -   Dynamic Links (iOS): Now fetches the invite ID when using universal
        links.
    -   Dynamic Links (iOS): Fixed crash on failure of dynamic link completion.
    -   Dynamic Links (iOS): Fixed an issue where some errors weren't correctly
        reported.
    -   Invites: Fixed SendInvite never completing in the stub implementation.
    -   Remote Config (iOS): Fixed an issue where some errors weren't correctly
        reported.
    -   Storage: Fixed Metadata::content_language returning the wrong data.
    -   Storage (iOS): Reference paths formats are now consistent with other
        platforms.
    -   Storage (iOS): Fixed an issue where trying to upload to a non-existent
        path would not complete the Future.
    -   Storage (iOS): Fixed a crash when a download fails.
    -   General (Windows): Updated all static libs to suppport different C
        runtime libraries and correspondingly updated the package directory
        structure.
    -   Linux: Fixed linking problems with all of the C++ stub libraries.

### 4.4.1

-   Overview
    -   Bug fixes for Realtime Database and Instance ID.
-   Changes
    -   Realtime Database: SetPersistenceEnabled now sets persistence enabled.
    -   Instance ID (iOS): GetToken no longer fails without an APNS certificate,
        and no longer forces registering for notifications.

### 4.4.0

-   Overview
    -   Support for Instance ID.
-   Changes
    -   Instance ID: Added Instance ID library.

### 4.3.0

-   Overview
    -   Bug fix for Remote Config and a new feature for Auth.
-   Changes
    -   Auth: Added support for accessing user metadata.
    -   Remote Config (Android): Fixed remote_config::ValueSource conversion.

### 4.2.0

-   Overview
    -   Bug fixes for Analytics, Database, and Messaging; and updates for Auth
        and Messaging.
-   Changes
    -   Analytics (iOS): Fixed a bug which prevented the user ID and user
        properties being cleared.
    -   Database (Android): Fixed MutableData::children_count().
    -   Messaging (Android): Fixed a bug which prevented the message ID field
        being set.
    -   Auth: Failed operations now return more specific error codes.
    -   Auth (iOS): Phone Authentication no longer requires push notifications.
        When push notifications aren't available, reCAPTCHA verification is used
        instead.
    -   Messaging: Messages sent to users can now contain a link URL.

### 4.1.0

-   Overview
    -   Bug fixes for AdMob, Auth, Messaging, Database, Storage, and Remote
        Config, and added features for Future's OnCompletion callbacks and
        Database transaction callbacks.
-   Changes
    -   General: Futures are now invalidated when their underlying Firebase API
        is destroyed.
    -   General: Added std::function support to Future::OnCompletion, to allow
        use of C++11 lambdas with captures.
    -   AdMob (iOS): Fixed a crash if a BannerView is deleted while a call to
        Destroy() is still pending.
    -   Auth (Android): Now assert fails if you call GetCredential without an
        Auth instance created.
    -   Database: DataSnapshot, DatabaseReference, Query, and other objects are
        invalidated when their Database instance is destroyed.
    -   Database: Added a context pointer to DatabaseReference::RunTransaction,
        as well as std::function support to allow use of C++11 lambdas with
        captures.
    -   Messaging (Android): Fixed a bug where message_type was not set in the
        Message struct.
    -   Messaging (iOS): Fixed a race condition if a message is received before
        Firebase Cloud Messaging is initialized.
    -   Messaging (iOS): Fixed a bug detecting whether the notification was
        opened if the app was running in the background.
    -   Remote Config: When listing keys, the list now includes keys with
        defaults set, even if they were not present in the fetched config.
    -   Storage: StorageReference objects are invalidated when their Storage
        instance is destroyed.
-   Known Issues
    -   When building on Android using STLPort, the std::function versions of
        Future::OnCompletion and DatabaseReference::RunTransaction are not
        available.

### 4.0.4

-   Changes
    -   Messaging (Android): Fixed a bug resulting in Messages not having their
        message_type field populated.

### 4.0.3

-   Overview
    -   Bug fixes for Dynamic Links, Messaging and iOS SDK compatibility.
-   Changes
    -   General (iOS): Fixed an issue which resulted in custom options not being
        applied to firebase::App instances.
    -   General (iOS): Fixed a bug which caused method implementation look ups
        to fail when other iOS SDKs rename the selectors of swizzled methods.
    -   Dynamic Links (Android): Fixed future completion if short link creation
        fails.
    -   Messaging (iOS): Fixed message handling when messages they are received
        via the direct channel to the FCM backend (i.e not via APNS).

### 4.0.2

-   Overview
    -   Bug fixes for Analytics, Auth, Dynamic Links, and Messaging.
-   Changes
    -   Analytics (Android): Fix SetCurrentScreen to work from any thread.
    -   Auth (iOS): Fixed user being invalidated when linking a credential
        fails.
    -   Dynamic Links: Fixed an issue which caused an app to crash or not
        receive a Dynamic Link if the link is opened when the app is installed
        and not running.
    -   Messaging (iOS): Fixed a crash when no Listener is set.
    -   Messaging: Fixed Listener::OnTokenReceived occasionally being called
        twice with the same token.

### 4.0.1

-   Overview
    -   Bug fixes for Dynamic links and Invites on iOS and Cloud Messaging on
        Android and iOS.
-   Changes
    -   Cloud Messaging (Android): Fixed an issue where Terminate was not
        correctly shutting down the Cloud Messaging library.
    -   Cloud Messaging (iOS): Fixed an issue where library would crash on start
        up if there was no registration token.
    -   Dynamic Links & Invites (iOS): Fixed an issue that resulted in apps not
        receiving a link when opening a link if the app is installed and not
        running.

### 4.0.0

-   Overview
    -   Added support for phone number authentication, access to user metadata,
        a standalone dynamic library and bug fixes.
-   Changes
    -   Auth: Added support for phone number authentication.
    -   Auth: Added the ability to retrieve user metadata.
    -   Auth: Moved token notification to a separate listener object.
    -   Dynamic Links: Added a standalone library separate from Invites.
    -   Invites (iOS): Fixed an issue in the analytics SDK's method swizzling
        which resulted in dynamic links / invites not being sent to the
        application.
    -   Messaging (Android): Fixed a regression introduced in 3.0.3 which caused
        a crash when opening up a notification when the app is running in the
        background.
    -   Messaging (iOS): Fixed interoperation with other users of local
        notifications.
    -   General (Android): Fixed crash in some circumstances after resolving
        dependencies by updating Google Play services.

### 3.1.2

-   Overview
    -   Bug fixes for Auth.
-   Changes
    -   Auth: Fixed a crash caused by a stale memory reference when a
        firebase::auth::Auth object is destroyed and then recreated for the same
        App object.
    -   Auth: Fixed potential memory corruption when AuthStateListener is
        destroyed. ### 3.1.1
-   Overview
    -   Bug fixes for Auth, Invites, Messaging, and Storage, plus a general fix.
-   Changes
    -   General (Android): Fixed Google Play Services updater crash when
        clicking outside of the dialog on Android 4.x devices.
    -   Auth: Fixed user being invalidated when linking a credential fails.
    -   Auth: Deprecated User::refresh_token().
    -   Messaging: Fixed incorrectly notifying the app of a message when a
        notification is received while the app is in the background and the app
        is then opened by via the app icon rather than the notification.
    -   Invites (iOS): Fixed an issue which resulted in the app delegate method
        application:openURL:sourceApplication:annotation: not being called when
        linking the invites library.
    -   Storage: Fixed a bug that prevented the construction of Metadata without
        a storage reference.

### 3.1.0

-   Overview
    -   Added support for multiple storage buckets in Cloud Storage for
        Firebase, and fixed a bug in Invites.
-   Changes
    -   Storage: Renamed "Firebase Storage" to "Cloud Storage for Firebase".
    -   Storage: Added an overload for `Storage::GetInstance()` that accepts a
        `gs://...` URL, so you can use Cloud Storage with multiple buckets.
    -   Invites: (Android) Fixed an issue where invites with empty links would
        fail to be received.

### 3.0.0

-   Overview
    -   Renamed some methods, fixed some bugs, and added some features.
-   Changes
    -   General: Renamed and deprecated methods that were inconsistent with the
        Google C++ Style Guide. Deprecated methods will be removed in a future
        release (approximately 2-3 releases from now).
    -   Analytics: Added `SetCurrentScreen()`.
    -   Auth: Fixed a race condition accessing user data in callbacks.
    -   Auth: (Android) Added `is_valid()` to check if a credential returned by
        `GetCredential()` is valid.
    -   Invites: (Android) Added a `Fetch()` function to fetch incoming
        invitations at times other than application start. You must call this on
        Android when your app returns to the foreground (on iOS, this is handled
        automatically).
    -   Messaging: Added a field to `firebase::messaging::Message` specifying
        whether the message was received when the app was in the background.
    -   Messaging: (Android) Added an AAR file containing the Android manifest
        changes needed for receiving notifications. You can add this to your
        project instead of modifying the manifest directly.
    -   Messaging: (iOS) Fixed regression since 2.1.1 that broke messaging on
        iOS 8 & 9 when an AppDelegate did not implement remote notification
        methods.
    -   Invites: (iOS) Fixed regression since 2.1.1 that broke invites if the
        AppDelegate did not implement the open URL method.
    -   Remote Config: Added support for initializing Remote Config defaults
        from `firebase::Variant` values, including binary data.

### 2.1.3

-   Overview
    -   Bug fixes for Auth and Messaging, and a fix for Future callbacks.
-   Changes
    -   General: Fixed a potential deadlock when running callbacks registered
        via `firebase::Future::OnCompletion()`.
    -   Auth: (Android) Fixed an error in `firebase::auth::User::PhotoUri()`.
    -   Messaging: (Android) Fixed an issue where a blank message would appear.
    -   Messaging: (iOS) Removed hard dependency on Xcode 8.

### 2.1.2

-   Overview
    -   Bug fix for AdMob on Android.
-   Changes
    -   AdMob: (Android) Fixed an issue in `firebase::admob::InterstitialAd`
        that caused a crash after displaying multiple interstitial ads.

### 2.1.1

-   Overview
    -   Bug fixes for Firebase Authentication, Messaging and Invites.
-   Changes
    -   Auth: (Android) Fixed an issue that caused a future to never complete
        when signing in while a user is already signed in.
    -   Messaging / Invites: (iOS) Fixed an issue with method swizzling that
        caused some of the application's UIApplicationDelegate methods to not be
        called.
    -   Messaging: (iOS) Fixed a bug which caused a crash when initializing the
        library when building with Xcode 8 for iOS 10.

### 2.1.0

-   Overview
    -   Support for Firebase Storage and minor bugfixes in other libraries.
-   Changes
    -   Storage: Added the Firebase Storage C++ client library.
    -   Auth: Added a check for saved user credentials when Auth is initialized.
-   Known Issues
    -   Storage: On Android, pausing and resuming storage operations will cause
        the transfer to fail with the error code kErrorUnknown.

### 2.0.0

-   Overview
    -   Support for AdMob Native Express Ads, Realtime Database and simplified
        the Invites API.
-   Changes
    -   AdMob: Added support for AdMob Native Express Ads.
    -   Auth: Added AuthStateListener class which provides notifications when a
        user is logged in or logged out.
    -   Realtime Database: Added a client library.
    -   Invites: Breaking change which significantly simplifies the API.
-   Known Issues
    -   AdMob: When calling Initialize, the optional admob_app_id argument is
        ignored.

### 1.2.1

-   Overview
    -   Bug fixes in Messaging.
-   Changes
    -   Messaging: Fixed a bug that prevented Android apps from terminating
        properly.
    -   Messaging: Added missing copy constructor implementation in iOS and stub
        libraries.

### 1.2.0

-   Overview
    -   New features in AdMob, Authentication, Messaging, and Remote Config, a
        helper class for initialization, and bug fixes.
-   Changes
    -   General: Added firebase::ModuleInitializer, a helper class to initialize
        Firebase modules and handle any missing dependency on Google Play
        services on Android.
    -   AdMob: Added Rewarded Video feature. For more information, see the
        [Rewarded Video C++ guide](https://firebase.google.com/docs/admob/cpp/rewarded-video).
    -   AdMob: You can now pass your AdMob App ID to
        firebase::admob::Initialize() to help reduce latency for the initial ad
        request.
    -   AdMob: On both iOS and Android, you must call BannerView::Show() to
        display the ad. Previously, this was only required on Android.
    -   AdMob: Fixed an issue where BannerView::Listener received an incorrect
        bounding box.
    -   AdMob: BannerView now has a black background, rather than transparent.
    -   Authentication: Implemented User::SendEmailVerification() and
        User::EmailVerified() methods on Android.
    -   Invites: Fixed a bug that occurred when initializing InvitesSender and
        InvitesReceiver at the same time.
    -   Invites: Fixed a potential crash at app shutdown on iOS when
        InvitesReceiver::Fetch() is pending.
    -   Messaging: Added firebase::messaging::Notification and associated
        methods for retrieving the contents of a notification on Android and
        iOS.
    -   Messaging: Added support for iOS 10 notifications.
    -   Messaging: Fixed a crash that occurred on Android if Messaging was
        initialized before the native library was loaded.
    -   RemoteConfig: Added GetKeys() and GetKeysByPrefix() methods, which get a
        list of the app's Remote Config parameter keys.

### 1.1.0

-   Overview
    -   Minor bug fixes and new way of checking Google Play services
        availability.
-   Changes
    -   Reverted the firebase::App changes from version 1.0.1 relating to Google
        Play services; this has been replaced with a new API.
    -   Each Firebase C++ library that requires Google Play services now checks
        for its availability at initialization time. See "Requiring Google Play
        services on Android".
        -   firebase::auth::GetAuth() now has an optional output parameter that
            indicates whether initialization was successful, and will return
            nullptr if not.
        -   firebase::messaging::Initialize() now returns a result that
            indicates whether initialization was successful.
        -   Added firebase::invites::Initialize(), which you must call once
            prior to creating InvitesSender or InvitesReceiver instances. This
            function returns a result that indicates whether initialization was
            successful.
        -   firebase::remote_config::Initialize() now returns a result that
            indicates whether initialization was successful.
        -   firebase::admob::Initialize() now returns a result that indicates
            whether initialization was successful.
    -   Added utility functions to check whether Google Play services is
        available. See google_play_services::CheckAvailability() and
        google_play_services::MakeAvailable() for more information.
-   Known Issues
    -   Invites: If you call InvitesReceiver::Fetch() or
        InvitesReceiver::ConvertInvitation() without first calling
        firebase::invites::Initialize(), the operation will never complete. To
        work around the issue, ensure that firebase::invites::Initialize() is
        called once before creating any InvitesReceiver instances.

### 1.0.1

-   Overview
    -   Minor bug fixes.
-   Changes
    -   Modified firebase::App to check for the required version of Google Play
        services on creation to prevent firebase::App creation failing if a
        user's device is out of date. If Google Play services is out of date, a
        dialog will prompt the user to install a new version. See "Requiring
        Google Play services on Android". With the previous version (version
        1.0.0) the developer needed to manually check for an up to date Google
        Play services using GoogleApiClient.
    -   Fixed potential deadlock when using SetListener from a notification
        callback in firebase::admob::InterstitialAd and
        firebase::admob::BannerView on iOS.
    -   Fixed race condition on destruction of admob::BannerView on Android.
    -   Fixed Future handle leak. An internal memory leak would manifest for
        objects or modules that use futures for the lifetime of the object or
        module. For example, during the lifetime of BannerView each call to a
        method which returns a Future could potentially allocate memory which
        wouldn't be reclaimed until the BannerView is destroyed.

### 1.0.0

-   Overview
    -   First public release. See our
        [setup guide](https://firebase.google.com/docs/cpp/setup) to get
        started.
-   Known Issues
    -   Android armeabi libraries must be linked with gcc 4.8.
