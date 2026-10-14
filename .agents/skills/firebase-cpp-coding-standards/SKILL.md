---
name: firebase-cpp-coding-standards
description:
  Coding standards, style conventions, and best practices for the Firebase C++
  SDK. Use when implementing new C++ features, writing cross-platform code,
  managing Android JNI references, formatting 64-bit integers, handling
  errors, and adhering to repository lint rules.
---

# Firebase C++ Coding Standards & Style Guide

This skill documents common coding standards, language patterns, and style rules
enforced across the Firebase C++ SDK repository.

## 1. Fixed-Width Integer Types & Formatted Output

### Use `<cstdint>` Types, Never Plain C `long`
The repository enforces the Google C++ Style Guide rule `[runtime/int]` via `cpplint`.
Plain C `long`, `unsigned long`, and `unsigned long long` must NOT be used.
Always use standard fixed-width types from `<cstdint>`:
- `int32_t`, `uint32_t`
- `int64_t`, `uint64_t`
- `int16_t`, `uint16_t`, `uint8_t`

### Formatted Printing of 64-bit Integers (`<cinttypes>`)
When printing 64-bit integers with `printf` or similar format functions, do NOT cast to `unsigned long long` with `%llu` (which violates the `[runtime/int]` rule). Instead, `#include <cinttypes>` and use standard format macros (`PRIu64`, `PRId64`, etc.):

```cpp
#include <cinttypes>
#include <cstdint>

uint64_t timestamp = token_result->auth_timestamp;
printf("Auth timestamp: %" PRIu64 "\n", timestamp);
```

## 2. Include What You Use (IWYU)

Always explicitly `#include` every header for standard library types used in each `.h` and `.cc` file:
- Container types: `<map>`, `<vector>`, `<set>`, `<unordered_map>`, etc.
- Smart pointers & utilities: `<memory>`, `<utility>`, `<string>`
- Types: `<cstdint>`, `<cinttypes>`, `<cstddef>`

Do NOT rely on transitive includes (e.g. including a header that happens to include `<map>`). `cpplint` checks for `[build/include_what_you_use]` on changed files in CI.

## 3. Android JNI Memory & Local Reference Management

### Know Which JNI Helpers Delete Local References Automatically
When calling Java methods from C++ using JNI (`env->CallObjectMethod`, `env->GetObjectField`, `env->NewStringUTF`, etc.), the JVM creates a local JNI reference.

In `app/src/util_android.cc`:
- **`util::JniStringToString(env, j_str)` and `util::JniUriToString(env, j_uri)` ALREADY call `env->DeleteLocalRef(...)` internally!** Do **NOT** call `env->DeleteLocalRef(j_str)` again after `util::JniStringToString`, or you will cause a JNI double-free bug.
- **`util::JStringToString(env, j_str)` and `util::JavaMapToVariantMap(env, &map, j_map)` do NOT delete the local reference.** Callers must explicitly call `env->DeleteLocalRef(j_map)` after conversion.

```cpp
const jobject j_token = env->CallObjectMethod(
    result, tokenresult::GetMethodId(tokenresult::kGetToken));
util::CheckAndClearJniExceptions(env);
const jobject j_claims = env->CallObjectMethod(
    result, tokenresult::GetMethodId(tokenresult::kGetClaims));
util::CheckAndClearJniExceptions(env);

// JniStringToString releases j_token for us.
data->token = util::JniStringToString(env, j_token);

// JavaMapToVariantMap does NOT release j_claims; delete it manually.
if (j_claims != nullptr) {
  util::JavaMapToVariantMap(env, &data->claims, j_claims);
  env->DeleteLocalRef(j_claims);
}
```

### Desktop-Only vs Common Headers (`variant_util.h` / FlatBuffers)
`app/src/variant_util.h` (which provides `util::JsonToVariant` and includes `flatbuffers/flexbuffers.h`) is compiled **only on Desktop** (`app_desktop_SRCS`). Do not place source files that include `app/src/variant_util.h` in `common_SRCS`; put them in `desktop_SRCS` and use the native Android (`util::JavaMapToVariantMap`) and iOS (`util::NSDictionaryToStdMap`) SDK objects on mobile platforms.

## 4. Deserialization, Parsing & Diagnostic Logging

When parsing or decoding serialized data (such as JWT tokens, JSON payloads, or Firebase database/auth variants):
- Design parsers to return a status (e.g. `bool ParseIdToken(...)`) indicating success or failure.
- When an asynchronous promise or function completes, do not fail silently if parsing fails. Always log a warning using `LogWarning(...)` so failures can be diagnosed:

```cpp
if (current_token.IsValid()) {
  IdTokenResult id_token_result;
  if (!ParseIdToken(current_token.token(), &id_token_result)) {
    LogWarning("GetIdTokenResult: Failed to parse cached ID token.");
  }
  promise.CompleteWithResult(id_token_result);
  return promise.future();
}
```

## 5. Public API Documentation & Operator Overloads

Every member exposed in a public header under `src/include/firebase/` must have Doxygen documentation. This includes:
- Classes and structs
- All methods and member functions
- Public member variables
- **Operator overloads**: `operator==`, `operator!=`, `operator<`, etc.

Undocumented operators will fail the GitHub Actions `generated_docs_check` CI job. Always provide `/// @brief`, `@param[in]`, and `@return` tags:

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

## 6. Reviewing GitHub PR Feedback via Script

When addressing PR reviews and bot comments (such as lint warnings or Doxygen documentation issues), use the repository's helper script:

```bash
python3 scripts/print_github_reviews.py --branch <branch_name> --token "$(gh auth token)"
```

Use `--since <timestamp>` on subsequent runs to inspect only newly added comments.

## 7. Pre-Submit Verification Checklist

Before pushing changes to a branch or opening a PR:
1. **Format Code**: Run `python3 scripts/format_code.py -git_diff`
2. **Lint Check**: Run `python3 -m cpplint --filter=-build/include_subdir,-build/c++11,-readability/casting,-runtime/references,-whitespace/indent,-whitespace/line_length <modified_files>`
3. **Doxygen Docs Check**:
   ```bash
   cp docs/Doxyfile /tmp/Doxyfile
   echo INPUT = $(find */src/include/firebase/ -name '*.h') >> /tmp/Doxyfile
   doxygen /tmp/Doxyfile 2>/tmp/doxygen_errors.txt
   grep -E "error:|warning:" /tmp/doxygen_errors.txt
   ```
