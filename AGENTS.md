# Introduction

> **Note on Document Formatting:** This document (`AGENTS.md`) should be
> maintained with lines word-wrapped to a maximum of 80 characters to ensure
> readability across various editors and terminals.

This document provides context and guidance for AI agents (like Jules) when
making changes to the Firebase C++ SDK repository. It covers essential
information about the repository's structure, setup, testing procedures, API
surface, best practices, and common coding patterns.

For a detailed view of which Firebase products are supported on each C++
platform (Android, iOS, tvOS, macOS, Windows, Linux), refer to the official
[Firebase library support by platform table](https://firebase.google.com/docs/cpp/learn-more#library-support-by-platform).

The Firebase C++ SDKs for desktop platforms (Windows, Linux, macOS) are
entirely open source and hosted in the main `firebase/firebase-cpp-sdk` GitHub
repository. The C++ SDKs for mobile platforms (iOS, tvOS, Android) are built
on top of the respective native open-source Firebase SDKs (Firebase iOS SDK and
Firebase Android SDK).

The goal is to enable agents to understand the existing conventions and
contribute effectively to the codebase.

# Setup Commands

## Prerequisites

Before building the Firebase C++ SDK, ensure the following prerequisites are
installed. Refer to the main `README.md` for detailed installation
instructions for your specific platform.

*   **CMake**: Version 3.7 or newer.
*   **Python**: Version 3.7 or newer.
*   **Abseil-py**: Python package.
*   **OpenSSL**: Required for desktop builds, unless you build with the
    `-DFIREBASE_USE_BORINGSSL=YES` cmake flag.
*   **libsecret-1-dev**: (Linux Desktop) Required for secure credential storage.
    Install using `sudo apt-get install libsecret-1-dev`.
*   **Android SDK & NDK**: Required for building Android libraries. `sdkmanager`
    can be used for installation. CMake for Android (version 3.10.2
    recommended) is also needed.
*   **(Windows Only) Strings**: From Microsoft Sysinternals, required for
    Android builds on Windows.
*   **Cocoapods**: Required for building iOS or tvOS libraries.

To build for Desktop, you can install prerequisites by running the following
script in the root of the repository: `scripts/gha/install_prereqs_desktop.py`

To build for Android, you can install prerequisites by running the following
script in the root of the repository: `build_scripts/android/install_prereqs.sh`

## Building the SDK

The SDK uses CMake for C++ compilation and Gradle for Android-specific parts.

### CMake (Desktop, iOS, tvOS)

1.  Create a build directory (e.g., `mkdir desktop_build && cd desktop_build`).
2.  Run CMake to configure: `cmake ..`
    *   For Desktop: Run as is. You can use BORINGSSL instead of OpenSSL (for fewer
        system dependencies with the `-DFIREBASE_USE_BORINGSSL=YES` parameter.
    *   For iOS, include the `-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/ios.cmake`
        parameter. This requires running on a Mac build machine.
3.  Build specific targets: `cmake --build . --target firebase_analytics`
    (replace `firebase_analytics` with the desired library).
    Or omit the entire `--target` parameter to build all targets.

You can also use the `scripts/gha/build_desktop.py` script to build the full
desktop SDK.

Refer to `README.md` for details on CMake generators and providing custom
third-party dependency locations.

### Gradle (Android)

Each Firebase C++ library is a Gradle subproject. To build a specific library
(e.g., Analytics):

```bash
./gradlew :analytics:assembleRelease
```

This command should be run from the root of the repository. Proguard files are
generated in each library's build directory (e.g.,
`analytics/build/analytics.pro`).

You can build the entire SDK for Android by running `./gradlew build` or
`build_scripts/android/build.sh`.

### Desktop Platform Setup Details

When setting up for desktop, if you are using an iOS
`GoogleService-Info.plist` file, convert it to the required
`google-services-desktop.json` using the script:
`python generate_xml_from_google_services_json.py --plist -i GoogleService-Info.plist`
(run this from the script's directory, ensuring the plist file is accessible).

The desktop SDK searches for configuration files in the current working
directory, first for `google-services-desktop.json`, then for
`google-services.json`.

Common system library dependencies for desktop:
*   **Windows**: Common dependencies include `advapi32.lib`, `ws2_32.lib`,
    `crypt32.lib`. Specific products might need others (e.g., Firestore:
    `rpcrt4.lib`, `ole32.lib`, `shell32.lib`).
*   **macOS**: Common dependencies include `pthread` (system library) and
    frameworks like `CoreFoundation`, `Foundation`, and `Security`.
*   **Linux**: Common dependencies include `pthread` (system library). When
    using GCC 5+, define `-D_GLIBCXX_USE_CXX11_ABI=0`.

## Including the SDK in Projects

### CMake Projects

Use `add_subdirectory()` in your `CMakeLists.txt`:

```cmake
add_subdirectory("[[Path to the Firebase C++ SDK]]")
target_link_libraries([[Your CMake Target]] firebase_analytics firebase_app)
```

### Android Gradle Projects

In addition to CMake setup, use `Android/firebase_dependencies.gradle` in your
`build.gradle`:

```gradle
apply from: "[[Path to the Firebase C++ SDK]]/Android/firebase_dependencies.gradle"
firebaseCpp.dependencies {
  analytics
}
```

For more detailed instructions and examples, always refer to the main
`README.md` and the
[C++ Quickstarts](https://github.com/firebase/quickstart-cpp).

# Testing

## Testing Strategy

The primary method for testing in this repository is through **integration
tests** for each Firebase library. While the `README.md` mentions unit tests
run via CTest, the current and preferred approach is to ensure comprehensive
coverage within the integration tests.

## Running Tests

*   **Integration Test Location**: Integration tests for each Firebase product
    (e.g., Firestore, Auth) are typically located in the `integration_test/`
    directory within that product's module (e.g.,
    `firestore/integration_test/`).
*   **Test Scripts**: The root of the repository contains scripts for running
    tests on various platforms, such as:
    *   `test_windows_x32.bat`
    *   `test_windows_x64.bat`
    *   `test_linux.sh`
    *   `test_mac_x64.sh`
    *   `test_mac_ios.sh`
    *   `test_mac_ios_simulator.sh`

    These scripts typically build the SDKs and then execute the relevant tests
    (primarily integration tests) via CTest or other platform-specific test
    runners.

## Writing Tests

When adding new features or fixing bugs:

*   Prioritize adding or updating integration tests within the respective
    product's `integration_test/` directory.
*   Ensure tests cover the new functionality thoroughly and verify interactions
    with the Firebase backend or other relevant components.
*   Follow existing patterns within the integration tests for consistency.

# API Surface

## General API Structure

The Firebase C++ SDK exposes its functionality through a set of classes and
functions organized by product (e.g., Firestore, Authentication, Realtime
Database).

### Initialization

1.  **`firebase::App`**: This is the central entry point for the SDK.
    *   It must be initialized first using `firebase::App::Create(...)`.
    *   On Android, this requires passing the JNI environment (`JNIEnv*`) and
        the Android Activity (`jobject`).
    *   `firebase::AppOptions` can be used to configure the app with specific
        parameters if not relying on a `google-services.json` or
        `GoogleService-Info.plist` file.
2.  **Service Instances**: Once `firebase::App` is initialized, you generally
    obtain instances of specific Firebase services using a static
    `GetInstance()` method on the service's class, passing the `firebase::App`
    object.
    *   Examples for services like Auth, Database, Storage, Firestore:
        *   `firebase::auth::Auth* auth = firebase::auth::Auth::GetAuth(app, &init_result);`
        *   `firebase::database::Database* database = firebase::database::Database::GetInstance(app, &init_result);`
        *   `firebase::storage::Storage* storage = firebase::storage::Storage::GetInstance(app, &init_result);`
    *   Always check the `init_result` (an `InitResult` enum, often
        `firebase::kInitResultSuccess` on success) to ensure these services
        were initialized successfully.
    *   **Note on Analytics**: Some products, like Firebase Analytics, have a
        different pattern. Analytics is typically initialized with
        `firebase::analytics::Initialize(const firebase::App& app)` (often
        handled automatically for the default `App` instance). After this,
        Analytics functions (e.g., `firebase::analytics::LogEvent(...)`) are
        called as global functions within the `firebase::analytics` namespace,
        rather than on an instance object obtained via `GetInstance()`.
        Refer to the specific product's header file for its exact
        initialization mechanism if it deviates from the common
        `GetInstance(app, ...)` pattern.

### Asynchronous Operations: `firebase::Future<T>`

All asynchronous operations in the SDK return a `firebase::Future<T>` object,
where `T` is the type of the expected result.

*   **Status Checking**: Use `future.status()` to check if the operation is
    `kFutureStatusPending`, `kFutureStatusComplete`, or
    `kFutureStatusInvalid`.
*   **Getting Results**: Once `future.status() == kFutureStatusComplete`:
    *   Check for errors: `future.error()`. A value of `0` (e.g.,
        `firebase::auth::kAuthErrorNone`,
        `firebase::database::kErrorNone`) usually indicates success.
    *   Get the error message: `future.error_message()`.
    *   Get the result: `future.result()`. This returns a pointer to the result
        object of type `T`. The result is only valid if `future.error()`
        indicates success.
*   **Completion Callbacks**: Use `future.OnCompletion(...)` to register a
    callback function (lambda or function pointer) that will be invoked when
    the future completes. The callback receives the completed future as an
    argument.

### Core Classes and Operations (Examples from Auth and Database)

While each Firebase product has its own specific classes, the following
examples illustrate common API patterns:

*   **`firebase::auth::Auth`**: The main entry point for Firebase
    Authentication.
    *   Used to manage users, sign in/out, etc.
    *   Successful authentication operations (like
        `SignInWithEmailAndPassword()`) return a
        `Future<firebase::auth::AuthResult>`. The `firebase::auth::User`
        object can then be obtained from this `AuthResult` (e.g.,
        `auth_result.result()->user()` after `result()` is confirmed
        successful and the pointer is checked).
    *   Example: `firebase::auth::User* current_user = auth->current_user();`
    *   Methods for user creation/authentication:
        `CreateUserWithEmailAndPassword()`, `SignInWithEmailAndPassword()`,
        `SignInWithCredential()`.
*   **`firebase::auth::User`**:
    *   Represents a user account. Data is typically accessed via its methods.
    *   Methods for profile updates: `UpdateEmail()`, `UpdatePassword()`,
        `UpdateUserProfile()`.
    *   Other operations: `SendEmailVerification()`, `Delete()`.
*   **`firebase::database::Database`**: The main entry point for Firebase
    Realtime Database.
    *   Used to get `DatabaseReference` objects to specific locations in the
        database.
*   **`firebase::database::DatabaseReference`**:
    *   Represents a reference to a specific location (path) in the Realtime
        Database.
    *   Methods for navigation: `Child()`, `Parent()`, `Root()`.
    *   Methods for data manipulation: `GetValue()`, `SetValue()`,
        `UpdateChildren()`, `RemoveValue()`.
    *   Methods for listeners: `AddValueListener()`, `AddChildListener()`.
*   **`firebase::database::Query`**:
    *   Used to retrieve data from a Realtime Database location based on
        specific criteria. Obtained from a `DatabaseReference`.
    *   **Filtering**: `OrderByChild()`, `OrderByKey()`, `OrderByValue()`,
        `EqualTo()`, `StartAt()`, `EndAt()`.
    *   **Limiting**: `LimitToFirst()`, `LimitToLast()`.
    *   Execution: `GetValue()` returns a `Future<DataSnapshot>`.
*   **`firebase::database::DataSnapshot`**: Contains data read from a Realtime
    Database location (either directly or as a result of a query). Accessed
    via `future.result()` or through listeners.
    *   Methods: `value()` (returns a `firebase::Variant` representing the
        data), `children_count()`, `children()`, `key()`, `exists()`.
*   **`firebase::Variant`**: A type that can hold various data types like
    integers, strings, booleans, vectors (arrays), and maps (objects),
    commonly used for reading and writing data with Realtime Database and other
    services.
*   **Operation-Specific Options**: Some operations might take optional
    parameters to control behavior, though not always through a dedicated
    "Options" class like Firestore's `SetOptions`. For example,
    `User::UpdateUserProfile()` takes a `UserProfile` struct.

### Listeners for Real-time Updates

Many Firebase services support real-time data synchronization using listeners.

*   **`firebase::database::ValueListener` /
    `firebase::database::ChildListener`**: Implemented by the developer and
    registered with a `DatabaseReference`.
    *   `ValueListener::OnValueChanged(const firebase::database::DataSnapshot& snapshot)`
        is called when the data at that location changes.
    *   `ChildListener` has methods like `OnChildAdded()`, `OnChildChanged()`,
        `OnChildRemoved()`.
*   **`firebase::auth::AuthStateListener`**: Implemented by the developer and
    registered with `firebase::auth::Auth`.
    *   `AuthStateListener::OnAuthStateChanged(firebase::auth::Auth* auth)` is
        called when the user's sign-in state changes.
*   **Removing Listeners**: Listeners are typically removed by passing the
    listener instance to a corresponding `Remove...Listener()` method (e.g.,
    `reference->RemoveValueListener(my_listener);`,
    `auth->RemoveAuthStateListener(my_auth_listener);`).

This overview provides a general understanding. Always refer to the specific
header files in `firebase/app/client/cpp/include/firebase/` and
`firebase/product_name/client/cpp/include/firebase/product_name/` for detailed
API documentation.

# Best Practices

## Coding Style

*   **Firebase C++ Style Guide**: For specific C++ API design and coding
    conventions relevant to this SDK, refer to the
    [STYLE_GUIDE.md](STYLE_GUIDE.md).
*   **Google C++ Style Guide**: Adhere to the
    [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
    as mentioned in `CONTRIBUTING.md`.
*   **Formatting**: Use `python3 scripts/format_code.py -git_diff -verbose` to
    format your code before committing.
*   **Naming Precision for Dynamic Systems**: Function names should precisely
    reflect their behavior, especially in systems with dynamic or asynchronous
    interactions. For example, a function that processes a list of items should
    be named differently from one that operates on a single, specific item
    captured asynchronously. Regularly re-evaluate function names as
    requirements evolve to maintain clarity.

## Comments

*   Write clear and concise comments where necessary to explain complex logic or
    non-obvious behavior.
*   **Avoid overly verbose comments**: Do not state the obvious. The code should
    be as self-documenting as possible.
*   Follow existing comment styles within the module you are working on.
*   "Avoid adding comments next to `#include` directives merely to explain why
    the include is necessary; the code usage should make this clear, or it can
    be part of a broader comment block if truly non-obvious for a section of
    code."
*   "Do not include comments that narrate the AI agent's iterative development
    process (e.g., 'Removed old logic here', 'Changed variable name from X to
    Y because...', 'Attempted Z but it did not work'). Comments should focus on
    explaining the current state of the code for future maintainers, not its
    development history or the AI's thought process."

## Error Handling

*   **Check `Future` status and errors**: Always check `future.status()` and
    `future.error()` before attempting to use `future.result()`.
    *   A common success code is `0` (e.g.,
        `firebase::auth::kAuthErrorNone`,
        `firebase::database::kErrorNone`). Other specific error codes are
        defined per module (e.g.,
        `firebase::auth::kAuthErrorUserNotFound`).
*   **Callback error parameters**: When using listeners or other callbacks,
    always check the provided error code and message before processing the
    data.
*   Provide meaningful error messages if your code introduces new error
    conditions.

## Resource Management

*   **`firebase::Future`**: `Future` objects manage their own resources.
    `Future::Release()` can be called, but it's often handled by RAII when
    futures go out of scope. Be mindful of `Future` lifetimes if they are
    stored as members or passed around.
*   **ListenerRegistrations**: When a listener is no longer needed, call
    `ListenerRegistration::Remove()` to detach it. Failure to do so can lead
    to memory leaks and unnecessary network/CPU usage.
*   **Pointers**: Standard C++ smart pointers (`std::unique_ptr`,
    `std::shared_ptr`) should be used where appropriate for managing
    dynamically allocated memory.
*   **`Future` Lifecycle**: Ensure `Future` objects returned from API calls are
    properly managed. While `Future`s handle their own internal memory for the
    result, the asynchronous operations they represent need to complete to
    reliably free all associated operational resources or to ensure actions
    (like writes to a database) are definitely finalized. Abandoning a `Future`
    (letting it go out of scope without checking its result, attaching an
    `OnCompletion` callback, or explicitly `Wait()`ing for it) can sometimes
    lead to operations not completing as expected or resources not being
    cleaned up promptly by the underlying services, especially if the `Future`
    is the only handle to that operation. Prefer using `OnCompletion` or
    otherwise ensuring the `Future` completes its course, particularly for
    operations with side effects or those that allocate significant backend
    resources.
*   **Lifecycle of Queued Callbacks/Blocks**: If blocks or callbacks are queued
    to be run upon an asynchronous event (e.g., an App Delegate class being set
    or a Future completing), clearly define and document their lifecycle.
    Determine if they are one-shot (cleared after first execution) or
    persistent (intended to run for multiple or future events). This impacts
    how associated data and the blocks themselves are stored and cleared,
    preventing memory leaks or unexpected multiple executions.

## Immutability

*   Be aware that some objects, like `firebase::firestore::Query`, are
    immutable. Methods that appear to modify them (e.g., `query.Where(...)`)
    actually return a new instance with the modification.

## Platform-Specific Code

*   Firebase C++ SDK supports multiple platforms (Android, iOS, tvOS, desktop -
    Windows, macOS, Linux).
*   Platform-specific code is typically organized into subdirectories:
    *   `android/` (for Java/JNI related code)
    *   `ios/` (for Objective-C/Swift interoperability code)
    *   `desktop/` (for Windows, macOS, Linux specific implementations)
    *   `common/` (for C++ code shared across all platforms)
*   Use preprocessor directives (e.g., `#if FIREBASE_PLATFORM_ANDROID`,
    `#if FIREBASE_PLATFORM_IOS`) to conditionally compile platform-specific
    sections when necessary, but prefer separate implementation files where
    possible for better organization.

## Platform-Specific Considerations

*   **Realtime Database (Desktop)**: The C++ SDK for Realtime Database on
    desktop platforms (Windows, macOS, Linux) uses a REST-based
    implementation. This means that any queries involving
    `Query::OrderByChild()` require corresponding indexes to be defined in your
    Firebase project's Realtime Database rules. Without these indexes, queries
    may fail or not return expected results.
*   **iOS Method Swizzling**: Be aware that some Firebase products on iOS
    (e.g., Dynamic Links, Cloud Messaging) use method swizzling to
    automatically attach handlers to your `AppDelegate`. While this simplifies
    integration, it can occasionally be a factor to consider when debugging app
    delegate behavior or integrating with other libraries that also perform
    swizzling.
    When implementing or interacting with swizzling, especially for App Delegate
    methods like `[UIApplication setDelegate:]`:
        *   Be highly aware that `setDelegate:` can be called multiple times
            with different delegate class instances, including proxy classes
            from other libraries (e.g., GUL - Google Utilities). Swizzling
            logic must be robust against being invoked multiple times for the
            same effective method on the same class or on classes in a
            hierarchy. An idempotency check (i.e., if the method's current IMP
            is already the target swizzled IMP, do nothing more for that
            specific swizzle attempt) in any swizzling utility can prevent
            issues like recursion.
        *   When tracking unique App Delegate classes (e.g., for applying hooks
            or callbacks via swizzling), consider the class hierarchy. If a
            superclass has already been processed, processing a subclass for
            the same inherited methods might be redundant or problematic. A
            strategy to check if a newly set delegate is a subclass of an
            already processed delegate can prevent such issues.
        *   For code that runs very early in the application lifecycle on
            iOS/macOS (e.g., `+load` methods, static initializers involved in
            swizzling), prefer using `NSLog` directly over custom logging
            frameworks if there's any uncertainty about whether the custom
            framework is fully initialized, to avoid crashes during logging
            itself.

## Class and File Structure

*   Follow the existing pattern of internal and common classes within each
    Firebase library (e.g., `firestore/src/common`, `firestore/src/main`,
    `firestore/src/android`).
*   Public headers defining the API are typically in
    `src/include/firebase/product_name/`.
*   Internal implementation classes often use the `Internal` suffix (e.g.,
    `DocumentReferenceInternal`).

# Common Patterns

## Pimpl (Pointer to Implementation) Idiom

*   Many public API classes (e.g., `firebase::storage::StorageReference`,
    `firebase::storage::Metadata`) use the Pimpl idiom.
*   They hold a pointer to an internal implementation class (e.g.,
    `StorageReferenceInternal`, `MetadataInternal`).
*   This pattern helps decouple the public interface from its implementation,
    reducing compilation dependencies and hiding internal details.
*   When working with these classes, you will primarily interact with the public
    interface. Modifications to the underlying implementation are done in the
    `*Internal` classes.

A common convention for this Pimpl pattern in the codebase (e.g., as seen in
`firebase::storage::Metadata`) is that the public class owns the raw pointer to
its internal implementation (`internal_`). This requires careful manual memory
management:
*   **Creation**: The `internal_` object is typically allocated with `new` in
    the public class's constructor(s). For instance, the default constructor
    might do `internal_ = new ClassNameInternal(...);`, and a copy constructor
    would do `internal_ = new ClassNameInternal(*other.internal_);` for a
    deep copy.
*   **Deletion**: The `internal_` object is `delete`d in the public class's
    destructor (e.g., `delete internal_;`). It's good practice to set
    `internal_ = nullptr;` immediately after deletion if the destructor isn't
    the absolute last point of usage, or if helper delete functions are used
    (as seen with `MetadataInternalCommon::DeleteInternal` which nullifies the
    pointer in the public class instance before deleting).
*   **Copy Semantics**:
    *   **Copy Constructor**: If supported, a deep copy is usually performed. A
        new `internal_` instance is created, and the contents from the source
        object's `internal_` are copied into this new instance.
    *   **Copy Assignment Operator**: If supported, it must also perform a deep
        copy. This typically involves deleting the current `internal_` object,
        then creating a new `internal_` instance and copying data from the
        source's `internal_` object. Standard self-assignment checks should be
        present.
*   **Move Semantics**:
    *   **Move Constructor**: If supported, ownership of the `internal_` pointer
        is transferred from the source object to the new object. The source
        object's `internal_` pointer is then set to `nullptr` to prevent
        double deletion.
    *   **Move Assignment Operator**: Similar to the move constructor, it
        involves deleting the current object's `internal_`, then taking
        ownership of the source's `internal_` pointer, and finally setting the
        source's `internal_` to `nullptr`.
*   **Cleanup Registration (Advanced)**: In some cases, like
    `firebase::storage::Metadata`, there might be an additional registration
    with a central cleanup mechanism (e.g., via `StorageInternal`). This acts
    as a safeguard or part of a larger resource management strategy within the
    module, but the fundamental responsibility for creation and deletion in
    typical scenarios lies with the Pimpl class itself.

It's crucial to correctly implement all these aspects (constructors,
destructor, copy/move operators) when dealing with raw pointer Pimpls to
prevent memory leaks, dangling pointers, or double deletions.

## Namespace Usage

*   Code for each Firebase product is typically within its own namespace under
    `firebase`.
    *   Example: `firebase::firestore`, `firebase::auth`,
        `firebase::database`.
*   Internal or platform-specific implementation details might be in nested
    namespaces like `firebase::firestore::internal` or
    `firebase::firestore::android`.

## Futures for Asynchronous Operations

*   As detailed in the API Surface section, `firebase::Future<T>` is the
    standard way all asynchronous operations return their results.
*   Familiarize yourself with `Future::status()`, `Future::error()`,
    `Future::error_message()`, `Future::result()`, and
    `Future::OnCompletion()`.

## Internal Classes and Helpers

*   Each module often has a set of internal classes (often suffixed with
    `Internal` or residing in an `internal` namespace) that manage the core
    logic, platform-specific interactions, and communication with the Firebase
    backend.
*   Utility functions and helper classes are common within the `common/` or
    `util/` subdirectories of a product.

## Singleton Usage (Limited)

*   The primary singleton is `firebase::App::GetInstance()`.
*   Service entry points like `firebase::firestore::Firestore::GetInstance()`
    also provide singleton-like access to service instances (scoped to an `App`
    instance).
*   Beyond these entry points, direct creation or use of singletons for core
    data objects or utility classes is not a dominant pattern. Dependencies are
    generally passed explicitly.

## Platform Abstraction

*   For operations that differ by platform, there's often a common C++
    interface defined in `common/` or `main/`, with specific implementations
    in `android/`, `ios/`, and `desktop/` directories.
*   JNI is used extensively in `android/` directories for C++ to Java
    communication.
*   Objective-C++ (`.mm` files) is used in `ios/` directories for C++ to
    Objective-C communication.

## Listener Pattern

*   The `AddSnapshotListener`/`ListenerRegistration` pattern is common for
    features requiring real-time updates (e.g., Firestore, Realtime
    Database).

## Builder/Fluent Interface for Queries

*   Classes like `firebase::firestore::Query` use a fluent interface (method
    chaining) to construct complex queries by progressively adding filters,
    ordering, and limits. Each call typically returns a new instance of the
    query object.

# Updating This Document

This document is a living guide. As the Firebase C++ SDK evolves, new patterns
may emerge, or existing practices might change. If you introduce a new common
pattern, significantly alter a build process, or establish a new best practice
during your work, please take a moment to update this `AGENTS.md` file
accordingly.

Keeping this document current will greatly benefit future AI agents and human
developers working on this repository.

# Prompting Jules AI

## Recommended General Prompt Instruction

When working on this task, please consistently refer to the `AGENTS.md` guide
for all repository-specific conventions, including setup procedures, coding
style, common architectural patterns, and API usage. Pay close attention to the
testing strategies outlined, ensuring your implementation includes
comprehensive integration tests with detailed test cases in your plan. Implement
robust error handling for any new or modified public API methods, following the
patterns for `firebase::Future` error reporting. If the feature involves
platform-specific code, ensure the public API remains consistent across all
platforms, as discussed in `AGENTS.md`. Write clear, maintainable code,
adhering to the commenting guidelines, and if you need to add new third-party
dependencies, document the rationale and update build configurations according
to the established practices. Ensure your changes align with the overall best
practices detailed in `AGENTS.md`.

## Key Directives for Jules AI

*   **Prioritize `AGENTS.md`**: This document (`AGENTS.md`) contains
    repository-specific guidelines. Prioritize this information when making
    decisions about coding style, testing procedures, architectural patterns,
    and API usage.
*   **Adherence to Patterns**: Strive to follow the common patterns and best
    practices outlined here. This ensures consistency across the codebase.
*   **Clarification for Deviations**: If the existing patterns in `AGENTS.md` do
    not seem suitable for a specific task, or if a deviation is necessary,
    please explicitly state this in your plan or request clarification before
    proceeding with implementation.
*   **Updating `AGENTS.md`**: If you introduce a new, broadly applicable
    pattern, or if a significant change to the build process or best practices
    occurs as part of your task, please include a step in your plan to update
    `AGENTS.md` to reflect these changes.
*   **Testing**: Remember that integration tests are the primary method of
    testing. Ensure new features are accompanied by corresponding integration
    tests as described in the 'Testing' section of `AGENTS.md`.
*   **Commit Messages**: Follow standard commit message guidelines. A brief
    summary line, followed by a more detailed explanation if necessary.
*   **Tool Limitations & Path Specificity**: If codebase search tools (like
    `grep` or recursive `ls`) are limited or unavailable, and initial attempts
    to locate files/modules based on common directory structures are
    unsuccessful, explicitly ask for more specific paths rather than assuming a
    module doesn't exist or contains no relevant code.
