---
name: firebase-cpp-commenting-standards
description:
  Guidelines for writing Doxygen documentation for C++ and Objective-C public
  APIs in the Firebase C++ SDK. Use when modifying or creating public headers,
  classes, structs, methods, or operator overloads.
---

# Firebase C++ Commenting Standards

Guidelines for writing Doxygen documentation for C++ and Objective-C public APIs in the Firebase C++ SDK.

## Overview
All public-facing methods, classes, and properties should be documented using Doxygen-style triple-slash `///` or block comments `/** ... */`.

The repository's CI enforces complete Doxygen coverage for all headers in `src/include/firebase/`. Any missing documentation or warnings cause the `generated_docs_check` GitHub Actions job to fail.

## C++ Standards

### 1. General Format
Use `///` for single-line or multi-line brief comments.
Use `@brief` to explicitly define the summary.

```cpp
/// @brief A brief description of the class or function.
```

### 2. Classes and Structs
Classes and structs should have a summary of their purpose. Use `@see` for related links.

```cpp
/// @brief Options that control the creation of a Firebase App.
/// @if cpp_examples
/// @see firebase::App
/// @endif
class AppOptions { ... };
```

### 3. Methods and Functions
Always document parameters using `@param[in]` / `@param[out]` and return values using `@return` or `@returns`.

```cpp
/// @brief Load default options from the resource file.
///
/// @param options Options to populate from a resource file.
///
/// @return An instance containing the loaded options if successful.
static AppOptions* LoadDefault(AppOptions* options);
```

### 4. Operator Overloads
All public operator overloads (`operator==`, `operator!=`, `operator<`, etc.) defined in public headers MUST have Doxygen comments (`/// @brief ...`, `@param[in]`, `@return`). Omitting documentation on operators will trigger Doxygen warnings and cause the CI `generated_docs_check` job to fail.

```cpp
  /// @brief Equality operator.
  ///
  /// @param[in] other The other object to compare with.
  /// @return true if all fields match, false otherwise.
  bool operator==(const IdTokenResult& other) const;

  /// @brief Inequality operator.
  ///
  /// @param[in] other The other object to compare with.
  /// @return true if any field differs, false otherwise.
  bool operator!=(const IdTokenResult& other) const;
```

### 5. Conditional Documentation (SWIG/Unity)
For Unity/C# bindings, use `<SWIG>` tags and `@if swig_examples`. These are parsed by the SWIG commenting pipeline and ignored by the standard C++ Doxyfile.

```cpp
/// @if cpp_examples
/// To create a firebase::App object, the Firebase application identifier
/// and API key should be set using set_app_id() and set_api_key()
/// respectively.
/// @endif
/// <SWIG>
/// @if swig_examples
/// To create a FirebaseApp object, the Firebase application identifier
/// and API key should be set using AppId and ApiKey respectively.
/// @endif
/// </SWIG>
```

---

## Objective-C and iOS/tvOS Standards

Objective-C wrappers should also follow standard Doxygen conventions, especially if they are used to bridge to C++.

### 1. Class Properties

```objc
/// @brief The App ID of the Firebase App.
@property(nonatomic, readonly) NSString *appID;
```

---

## Validating Documentation Locally (Doxygen)

The GitHub Actions CI runs Doxygen and treats any warnings in public headers as hard errors. You can run the exact same check locally before creating a PR:

```bash
cp docs/Doxyfile /tmp/Doxyfile
echo INPUT = $(find */src/include/firebase/ -name '*.h') >> /tmp/Doxyfile
doxygen /tmp/Doxyfile 2>/tmp/doxygen_errors.txt
grep -E "error:|warning:" /tmp/doxygen_errors.txt
```

If this produces any output referencing your modified header files, fix the missing Doxygen tags before pushing.

---

## Checklist
- [ ] Are you using `///` for public API comments?
- [ ] Do you have a `@brief` for all classes, structs, methods, and functions?
- [ ] Are all operator overloads (`operator==`, `operator!=`, etc.) documented?
- [ ] Are all parameters documented with `@param`?
- [ ] Is the return value documented with `@return`?
- [ ] Are you using `<SWIG>` tags if the documentation needs to differ for Unity/C#?
- [ ] Did you verify Doxygen locally without warnings?
