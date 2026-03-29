---
name: firebase-cpp-builder
description:
  Instructions and workflows for building the Firebase C++ SDK natively. Use
  this skill when you need to compile the C++ SDK per-product (e.g., auth,
  database) across different platforms (Desktop, iOS, Android).
---

# Building the Firebase C++ SDK

This skill provides instructions on how to build the Firebase C++ SDK directly
from source, specifically tailored for building individual products (like
`firebase_auth`, `firebase_database`, etc.).

## 1. Desktop Builds (Mac, Linux, Windows)

The easiest way to build for Desktop is to use the Python helper script from the
repository root:

```bash
python3 scripts/gha/build_desktop.py -a <arch> --build_dir <dir>
```

_(Plenty more options: run `python3 scripts/gha/build_desktop.py --help` for
info.)_

Alternatively, you can build manually with CMake. The build process will
automatically download necessary dependencies.

```bash
mkdir desktop_build
cd desktop_build
cmake ..
cmake --build . --target firebase_<product> -j
```

_Example: `cmake --build . --target firebase_auth -j`_

## 2. iOS Builds

For iOS, you can use the convenience script from the repository root:

```bash
./build_scripts/ios/build.sh -b ios_build_dir -s .
```

_(Run `./build_scripts/ios/build.sh -h` for more options.)_

Alternatively, you can use CMake's native Xcode generator manually. Ensure you
have CocoaPods installed if building products that depend on iOS SDK Pods.

```bash
mkdir ios_build
cd ios_build
cmake .. -G Xcode -DCMAKE_SYSTEM_NAME=iOS
cmake --build . --target firebase_<product> -j
```

Alternatively, you can build XCFrameworks using the helper script:

```bash
python3 scripts/gha/build_ios_tvos.py -s . -b ios_tvos_build
```

## 3. Android Builds

For Android, use the convenience script from the repository root:

```bash
./build_scripts/android/build.sh android_build_dir .
```

_(You can also specify an STL variant with the 3rd parameter. Run the script
without any parameters to see the usage.)_

Alternatively, when building the Firebase C++ SDK manually for Android, the
repository uses a Gradle wrapper where each product has its own subproject.

To build a specific product's AARs, run the gradle build task for that module:

```bash
./gradlew :<product>:build
```

_Example: `./gradlew :auth:build` or `./gradlew :database:build`_

Note that as part of the build process, each library generates a ProGuard file
in its build directory (e.g., `<product>/build/<product>.pro`).

## CMake Target Naming Convention

When using CMake directly (for Desktop or iOS/tvOS), the targets are
consistently named `firebase_<product>`.

Common targets include:

- `firebase_app`
- `firebase_analytics`
- `firebase_auth`
- `firebase_database`
- `firebase_firestore`
- `firebase_functions`
- `firebase_messaging`
- `firebase_remote_config`
- `firebase_storage`

## Product Naming Disambiguation (`firebase_auth` vs `auth`)

Different build tools use different naming conventions for products in this repository:

- **CMake Targets (Desktop/iOS)**: Typically prefixed with `firebase_` (e.g., `firebase_auth`).
- **Gradle Subprojects (Android)**: Typically use the raw module name (e.g., `:auth:build`).
- **Python Scripts** (e.g., `build_desktop.py`, `build_testapps.py`): Typically use the raw module name (e.g., `--t auth`).

> [!TIP]
> If you are unsure about the exact product name or supported flags, run Python scripts with `--help` (e.g., `build_desktop.py --help`). For shell scripts, run without parameters (Android) or with `-h` (iOS) to see usage.
