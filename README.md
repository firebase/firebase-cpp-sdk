# Firebase C++ Open Source Development

The repository contains the Firebase C++ SDK source, with support for Android,
iOS, and desktop platforms. It includes the following Firebase libraries:

<img height="200" align="right" src="https://user-images.githubusercontent.com/94545831/196925526-b6f89f3e-0abb-4ea2-842b-371487196463.png" />

|||
|--|--|
|[Google Analytics for Firebase](https://firebase.google.com/docs/analytics/)|
|[Firebase Authentication](https://firebase.google.com/docs/auth/)|[Firebase Realtime Database](https://firebase.google.com/docs/database/)|
|[Firebase Dynamic Links](https://firebase.google.com/docs/dynamic-links/)|[Cloud Firestore](https://firebase.google.com/docs/firestore/)|
|[Cloud Functions for Firebase](https://firebase.google.com/docs/functions/)|[Firebase Invites](https://firebase.google.com/docs/invites/)|
|[Firebase Cloud Messaging](https://firebase.google.com/docs/cloud-messaging/)|[Firebase Remote Config](https://firebase.google.com/docs/remote-config/)|
|[Cloud Storage for Firebase](https://firebase.google.com/docs/storage/)|

<br>

Firebase is an app development platform with tools to help you build, grow and monetize your app. More information about Firebase can be found [HERE](https://firebase.google.com).

- More information about the Firebase C++ SDK can be found [HERE](https://firebase.google.com/docs/cpp/setup)
- Samples on how to use the Firebase C++ SDK can be found [HERE](https://github.com/firebase/quickstart-cpp)

<br>

![Github Repo Size](https://img.shields.io/github/repo-size/firebase/firebase-cpp-sdk?style=for-the-badge&color=aqua)

## Table of Contents

1. [Getting Started](#getting-started)
1. [Prerequisites](#prerequisites)
1. [Building](#building)
1. [Including in Projects](#including-in-projects)
1. [Contributing Guidelines](https://github.com/firebase/firebase-cpp-sdk/blob/main/CONTRIBUTING.md)
1. [License](#license)

## Getting Started
You can clone the repo with the following command:

``` bash
git clone https://github.com/firebase/firebase-cpp-sdk.git
```

<details><summary><h2>Prerequisites</h2></summary>

The following prerequisites are required for all platforms.  Be sure to add any
directories to your PATH as needed.

- [CMake](https://cmake.org/), version 3.7, or newer
- [Python](https://www.python.org/), version of 3.7, or newer
- [Abseil-py](https://github.com/abseil/abseil-py)

Note: Once python is installed you can use the following commands to install
required packages:

* python -m ensurepip --default-pip
* python -m pip install --user absl-py

### Prerequisites for Desktop
The following prerequisites are required when building the libraries for
desktop platforms.

- [OpenSSL](https://www.openssl.org/), needed for Realtime Database and Cloud Firestore

### Prerequisites for Windows
Prebuilt packages for openssl can be found using google and if CMake fails to
find the install path use the command line option
**-DOPENSSL_ROOT_DIR=[Open SSL Dir]**.

### Prerequisites for Mac
Home brew can be used to install required dependencies:

```bash
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
source $HOME/.rvm/scripts/rvm
brew install cmake python3
sudo chown -R $(whoami) /usr/local
```

### Prerequisites for Android
The following prerequisites are required when building the libraries for
Android.

- Android SDK, Android NDK, and CMake for Android (version 3.10.2 recommended)
  - Download sdkmanager (either independently, or as a part of Android Studio)
    [here](https://developer.android.com/studio/#downloads)
  - Follow [these instructions](https://developer.android.com/studio/projects/add-native-code.html#download-ndk)
    to install the necessary build tools
- (Windows only) [Strings (from Microsoft Sysinternals)](https://docs.microsoft.com/en-us/sysinternals/downloads/strings)
  > **Important - Strings EULA** \
  > You will have to run Strings once from the command line to accept the
    EULA before it will work as part of the build process.

Note that we include the Gradle wrapper, which if used will acquire the
necessary version of Gradle for you.

### Prerequisites for iOS/tvOS
The following prerequisites are required when building the libraries for iOS or tvOS.
- [Cocoapods](https://cocoapods.org/)

</details>


<details><summary><h2>Building</h2></summary>

### Building with CMake
The build uses CMake to generate the necessary build files, and supports out of
source builds.
The CMake following targets are available to build and link with:

| Feature | CMake Target |
| ------- | ------------ |
| App (base library) | firebase_app |
| Google Analytics for Firebase | firebase_analytics |
| Firebase Authentication | firebase_auth |
| Firebase Realtime Database | firebase_database |
| Firebase Dynamic Links | firebase_dynamic_links |
| Cloud Firestore | firebase_firestore |
| Cloud Functions for Firebase | firebase_functions |
| Firebase Invites | firebase_invites |
| Firebase Cloud Messaging | firebase_messaging |
| Firebase Remote Config | firebase_remote_config |
| Cloud Storage for Firebase | firebase_storage |

For example, to build the Analytics library, you could run the following
commands:

``` bash
mkdir desktop_build && cd desktop_build
cmake ..
cmake --build . --target firebase_analytics
```

Note that you can provide a different generator on the configure step, for
example to generate a project for Visual Studio 2017, you could run:

``` bash
cmake -G “Visual Studio 15 2017” ..
```

More information on generators can be found at
<https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html>.

By default, when building the SDK, the CMake process will download any third
party dependencies that are needed for the build. This logic is in
[cmake/external_rules.cmake](/cmake/external_rules.cmake), and the accompanying
[cmake/external/CMakeLists.txt](/cmake/external/CMakeLists.txt). If you would
like to provide your own directory for these dependencies, you can override
`[[dependency_name]]_SOURCE_DIR` and `[[dependency_name]]_BINARY_DIR`. If the
binary directory is not provided, it defaults to the given source directory,
appended with `-build`.

For example, to provide a custom flatbuffer directory you could run:

``` bash
cmake -DFLATBUFFERS_SOURCE_DIR=/tmp/flatbuffers ..
```

And the binary directory would automatically be set to `/tmp/flatbuffers-build`.

Currently, the third party libraries that can be provided this way are:

| Library |
| ------- |
| CURL |
| FLATBUFFERS |
| LIBUV |
| NANOPB |
| UWEBSOCKETS |
| ZLIB |

#### Building with CMake for iOS
The Firebase C++ SDK comes with a CMake config file to build the library for
iOS platforms, [cmake/toolchains/ios.cmake](/cmake/toolchains/ios.cmake).  In
order to build with it, when running the CMake configuration pass it in with
the CMAKE_TOOLCHAIN_FILE definition.  For example, to build the Auth
library for iOS, you could run the following commands:

``` bash
mkdir ios_build && cd ios_build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/ios.cmake ..
cmake --build . --target firebase_auth
```

#### Building with CMake for tvOS
The Firebase C++ SDK comes with a CMake config file to build the library for
tvOS platforms, [cmake/toolchains/apple.toolchain.cmake](/cmake/toolchains/apple.toolchain.cmake).  In
order to build with it, when running the CMake configuration pass it in with
the CMAKE_TOOLCHAIN_FILE definition.  For example, to build the Auth
library for tvOS, you could run the following commands:

``` bash
mkdir tvos_build && cd tvos_build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/apple.toolchain.cmake -DPLATFORM=TVOS ..
cmake --build . --target firebase_auth
```

#### Building XCFrameworks for both iOS and tvOS
The Firebase C++ SDK comes with a helper Python script to build XCFrameworks
that work for both iOS and tvOS. This is helpful as we can use the same
deliverable for both iOS and tvOS targets in the same XCode project.

``` bash
# Install prereqs (like cocoapods)
./build_scripts/tvos/install_prereqs.sh
python scripts/gha/build_ios_tvos.py -s . -b ios_tvos_build
```


### Building with Gradle for Android
When building the Firebase C++ SDK for Android, gradle is used in combination
with CMake when producing the libraries.  Each Firebase feature is its own
gradle subproject off of the root directory. The gradle target to build the
release version of each Firebase library is:

| Feature | Gradle Target |
| ------- | ------------- |
| App (base library) | :app:assembleRelease |
| Google Analytics for Firebase | :analytics:assembleRelease |
| Firebase Authentication | :auth:assembleRelease |
| Firebase Realtime Database | :database:assembleRelease |
| Firebase Dynamic Links | :dynamic_links:assembleRelease |
| Cloud Firestore | :firestore:assembleRelease |
| Cloud Functions for Firebase | :functions:assembleRelease |
| Firebase Invites | :invites:assembleRelease |
| Firebase Cloud Messaging | :messaging:assembleRelease |
| Firebase Remote Config | :remote_config:assembleRelease |
| Cloud Storage for Firebase | :storage:assembleRelease |

For example, to build the release version of the Analytics library, you could
run the following from the root directory:

``` bash
./gradlew :analytics:assembleRelease
```

#### Proguard File Generation
Note that as part of the build process, each library generates a proguard file
that should be included in your application.  The generated file is located in
each library’s build directory.  For example, the Analytics proguard file would
be generated to `analytics/build/analytics.pro`.

</details>

## Testing
Each Firebase SDK in this repo includes a series of unit tests. These tests are
built and executed by the CI system in order to validate changes and pull
requests.

The provided `test_windows_x32.bat`, `test_windows_x64.bat`,
`test_linux.sh` and `test_mac_x64.sh` scripts build the SDKs and execute
the unit tests via **ctest** on Windows32, Windows64, Linux and MacOS hosts,
respectively.  These scripts reside in the base directory of the repository.

### Known Issues
- Mac 
  - When executing tests you may be requested to unlock your Mac OS keychain.
    Please enter your keychain password and select **Always Allow**. If you
    still encounter repeated access request dialogs then you must unlock the
    keychain manually otherwise some tests will fail.
      - Open the **Keychain access** application on your Mac.
      - Under **Keychains** (upper left) select the **login** keychain.
      - Under **Category** select **Passwords** as a category (lower left) and
        find the entry **not_a_real_project_id.{hashcode}**.  Right click it.
      - Select **Get Info**, select **Access Control** and enable the 
        **Allow all applications to access this item** radio button.
      - Re-run the tests.

<details><summary><h2>Including in Projects</h2></summary>

### Including in CMake Projects
Including the Firebase C++ SDK to another CMake project is fairly
straightforward.  In the CMakeLists.txt file that wants to include the Firebase
C++ SDK, you can use
[add_subdirectory](https://cmake.org/cmake/help/latest/command/add_subdirectory.html),
providing the location of the cloned repository.  For example, to add
Analytics, you could add the following to your CMakeLists.txt file:

``` cmake
add_subdirectory( [[Path to the Firebase C++ SDK]] )
target_link_libraries( [[Your CMake Target]] firebase_analytics firebase_app)
```

Additional examples of how to do this for each library are available in the
[C++ Quickstarts](https://github.com/firebase/quickstart-cpp).

### Including in Android Gradle Projects
In order to link the Firebase C++ SDK with your gradle project, in addition to
the CMake instructions above, you can use
[Android/firebase_dependencies.gradle](/Android/firebase_dependencies.gradle)
to link the libraries, their dependencies, and the generated proguard files. For
example, to add Analytics, you could add the following to your build.gradle
file:

``` gradle
apply from: “[[Path to the Firebase C++ SDK]]/Android/firebase_dependencies.gradle”
firebaseCpp.dependencies {
  analytics
}
```

Additional examples of how to do this for each library are available in the
[C++ Quickstarts](https://github.com/firebase/quickstart-cpp).

</details>

## License
The contents of this repository is licensed under the
[Apache License, version 2.0](http://www.apache.org/licenses/LICENSE-2.0).

Your use of Firebase is governed by the
[Terms of Service for Firebase Services](https://firebase.google.com/terms/).

