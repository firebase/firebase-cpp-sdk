// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Generated from analytics.h by generate_windows_stubs.py

#ifndef FIREBASE_ANALYTICS_SRC_WINDOWS_ANALYTICS_DESKTOP_DYNAMIC_H_
#define FIREBASE_ANALYTICS_SRC_WINDOWS_ANALYTICS_DESKTOP_DYNAMIC_H_

#include <stdbool.h>  // needed for bool type in pure C

// --- Copied from original header ---
#include <stdint.h>
#include <stdbool.h>

// --- End of copied section ---

#ifdef __cplusplus
extern "C" {
#endif

// --- Function Pointer Declarations ---
// clang-format off
extern void (*ptr_MyTestFunction1)(int param);
extern bool (*ptr_MyTestFunction2)();

#define MyTestFunction1 ptr_MyTestFunction1
#define MyTestFunction2 ptr_MyTestFunction2
// clang-format on

// Number of Google Analytics functions expected to be loaded from the DLL.
extern const int FirebaseAnalytics_DynamicFunctionCount;

// --- Dynamic Loader Declaration for Windows ---
#if defined(_WIN32)

#include <windows.h>

// Array of known Google Analytics Windows DLL SHA256 hashes (hex strings).
extern const char* FirebaseAnalytics_KnownWindowsDllHashes[];
// Count of known Google Analytics Windows DLL SHA256 hashes.
extern const int FirebaseAnalytics_KnownWindowsDllHashCount;

// Load Analytics functions from the given DLL handle into function pointers.
// Returns the number of functions successfully loaded.
int FirebaseAnalytics_LoadDynamicFunctions(HMODULE dll_handle);

// Reset all function pointers back to stubs.
void FirebaseAnalytics_UnloadDynamicFunctions(void);

#endif  // defined(_WIN32)

#ifdef __cplusplus
}
#endif

#endif  // FIREBASE_ANALYTICS_SRC_WINDOWS_ANALYTICS_DESKTOP_DYNAMIC_H_
