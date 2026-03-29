---
name: firebase-cpp-test-runner
description:
  Workflows for locally building and running test apps for the Firebase C++ SDK
  across Android, iOS, and Desktop. Use when validating new features or bug
  fixes.
---

# Test Runner for Firebase C++ SDK

This skill outlines the workflows and convenience scripts required to locally
build and run tests for the Firebase C++ SDK across Android, iOS, and Desktop
platforms.

## Prerequisites

Before building integration tests, ensure that you have the private Google
Services configuration files present in the `integration_test/` directory of the
product you are testing:

- **Android and Desktop**: `google-services.json` must be present.
- **iOS**: `GoogleService-Info.plist` must be present.

_Note:_ If you are testing `auth`, you would place these files in
`auth/integration_test/google-services.json` and
`auth/integration_test/GoogleService-Info.plist`.

## Using the `build_testapps.py` Helper Script

The easiest and standard way to build integration tests locally across platforms
is to use the `build_testapps.py` script.

### Building for Android

To build an Android integration test application for specific products (e.g.,
`auth` and `database`), run the following command from the repository root:

```bash
python3 scripts/gha/build_testapps.py --p Android --t auth,database -output_directory ./android_testapp
```

This builds an Android app at
`build/outputs/apk/debug/integration_test-debug.apk` within the product's
module, which you can run on an Android emulator or physical device. Use
`./gradlew clean` to clean up the build artifacts.

### Building for iOS

To build iOS testapps for a specific product (e.g., `auth`):

```bash
python3 scripts/gha/build_testapps.py --t auth --p iOS
```

_Note:_ You must have a Mac environment with Xcode and Cocoapods to build iOS
tests successfully.

### Building for Desktop

You can use `build_testapps.py` or the provided platform-specific convenience
scripts for Desktop.

Alternatively, if you want to build the SDK manually via CMake:

```bash
python3 scripts/gha/build_desktop.py -a <arch> --build_dir <dir>
```

Use `python3 scripts/gha/build_desktop.py --help` for more options.

## Manual Integration Test Builds (Alternative)

If you need more control, you can build tests manually from within the
`integration_test/` (or `integration_test_internal/`) directories.

### Android Manual Build

```bash
cd <product>/integration_test
cp path_to_google_services_files/google-services.json .
export FIREBASE_CPP_SDK_DIR=path_to_cpp_git_repo
./gradlew build
```

### Desktop Manual Build

```bash
cd <product>/integration_test
cp path_to_google_services_files/google-services.json .
mkdir desktop_build && cd desktop_build
cmake .. && cmake --build . -j
```

Once the build is finished, run the generated `integration_test` binary.

## Product Naming Disambiguation (`firebase_auth` vs `auth`)

Different build tools use different naming conventions for products in this repository:

- **CMake Targets (Desktop/iOS)**: Typically prefixed with `firebase_` (e.g., `firebase_auth`).
- **Gradle Subprojects (Android)**: Typically use the raw module name (e.g., `:auth:build`).
- **Python Scripts** (e.g., `build_testapps.py`): Typically use the raw module name (e.g., `--t auth`).

> [!TIP]
> If you are unsure about the exact product name or supported flags, run Python scripts with `--help` (e.g., `build_testapps.py --help`). For shell scripts, run without parameters (Android) or with `-h` (iOS) to see usage.
