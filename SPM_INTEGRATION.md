# Building the Firebase C++ SDK with Swift Package Manager (SPM)

Historically, the Firebase C++ SDK used CocoaPods to download the necessary iOS native SDK dependencies during the CMake configure step. However, as CocoaPods is being deprecated in favor of Swift Package Manager (SPM), we have introduced native support for bridging SPM directly into our CMake workflow.

CMake and Swift Package Manager are independent build systems that do not natively communicate with each other. If you are building a Swift or C++ application with CMake and want to use external SPM packages, you generally choose between two paths: compiling the Swift code purely inside CMake, or bridging CMake to pull in SPM packages dynamically. 

Here is how we handle this integration.

## Method 1: The Recommended SPM Integration Approach

The Firebase C++ SDK CMake system provides a mechanism to automatically generate a `Package.swift` file, resolve its dependencies via `swift package resolve`, and link the resulting headers and libraries against your C++ targets. This completely bypasses the need for `pod install` or CocoaPods installations on the developer's machine.

### Enabling SPM Integration

When configuring the project for iOS using CMake, pass the `-DFIREBASE_IOS_USE_SPM=ON` flag:

```bash
mkdir -p mac_ios_build_xcode
cd mac_ios_build_xcode
cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS .. -DFIREBASE_IOS_USE_SPM=ON
```

*Note: The Xcode generator (`-G Xcode`) is required for proper Swift and Apple platform support within CMake.*

### How It Works Under The Hood

1. **Package.swift Generation**: Instead of forcing CMake to read a predefined `Package.swift` file, the `ios_pod/CMakeLists.txt` file reads the required iOS SDK versions (e.g., Firebase 12.14.0 and UMP 2.3.0) and dynamically writes a `Package.swift` manifest into the build directory.
2. **Package Resolution**: During the configure step, CMake executes `swift package resolve`. This fetches the source code of the `firebase-ios-sdk` and all of its transitive dependencies (such as `GoogleUtilities`, `PromisesObjC`, and `abseil`) directly into the `.build/checkouts/` folder.
3. **Header Discovery**: Instead of manually mapping each header path, CMake recursively globs the `.build/checkouts/` directory for any `/Public` or `/include` paths, and appends them to the C++ SDK's include directories.
4. **Header Overrides**: Files like `app/CMakeLists.txt` which historically hardcoded specific CocoaPods paths (e.g. `Pods/FirebaseFirestoreInternal/Firestore/core/`) now conditionally point to the SPM `.build/checkouts/` paths.

## Method 2: Legacy CocoaPods Build

If you are not yet ready to migrate your project's CI pipeline to SPM, you can continue to use the legacy CocoaPods integration. 

By simply omitting the `-DFIREBASE_IOS_USE_SPM=ON` flag, the CMake configuration will fallback to generating a `Podfile` and running `pod install`.

```bash
mkdir -p mac_ios_build_xcode
cd mac_ios_build_xcode
cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS ..
```

*Note: CocoaPods integration is considered legacy and will be fully deprecated in the future.*

## Unity SDK Considerations

When integrating with the Firebase Unity SDK, ensure that the External Dependency Manager for Unity (EDM4U) is configured to use SPM or Xcode workspace generation. EDM4U naturally supports Swift Package Manager dependencies using the `<swiftPackage>` tag in your dependency XML, which aligns with this C++ SDK upgrade.
