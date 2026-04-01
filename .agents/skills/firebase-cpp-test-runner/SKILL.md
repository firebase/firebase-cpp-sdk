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

Before building integration tests, ensure that your Python environment is set up with all required dependencies, and that you have the private Google Services configuration files present:

- **Python Dependencies**: Run `pip install -r scripts/gha/python_requirements.txt` to install the required libraries (such as `attrs`, `absl-py`, etc.).
- **Android and Desktop Config Files**: `google-services.json` must be present in `<product>/integration_test/`.
- **iOS Config Files**: `GoogleService-Info.plist` must be present in `<product>/integration_test/`.

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

To build a desktop integration test application for a specific product (e.g., `auth`), run the following command from the repository root:

```bash
python3 scripts/gha/build_testapps.py --p Desktop --t auth
```

You can specify the architecture with the `--arch` flag (e.g., `--arch x64`, `--arch x86`, or `--arch arm64`).

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
cmake .. -DFIREBASE_CPP_SDK_DIR=/path/to/firebase_cpp_sdk && cmake --build . -j
```

Once the build is finished, run the generated `integration_test` binary.

## Product Naming Disambiguation (`firebase_auth` vs `auth`)

Different build tools use different naming conventions for products in this repository:

- **CMake Targets (Desktop/iOS)**: Typically prefixed with `firebase_` (e.g., `firebase_auth`).
- **Gradle Subprojects (Android)**: Typically use the raw module name (e.g., `:auth:build`).
- **Python Scripts** (e.g., `build_testapps.py`): Typically use the raw module name (e.g., `--t auth`).

> [!TIP]
> If you are unsure about the exact product name or supported flags, run Python scripts with `--help` (e.g., `build_testapps.py --help`). For shell scripts, run without parameters (Android) or with `-h` (iOS) to see usage.
