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

#include "analytics/src/analytics_desktop_dynamic.h"

#include <stddef.h>

static void* g_stub_memory = NULL;

// clang-format off

// Number of Google Analytics functions expected to be loaded from the DLL.
const int FirebaseAnalytics_DynamicFunctionCount = 2;

#if defined(_WIN32)
// Array of known Google Analytics Windows DLL SHA256 hashes (hex strings).
const char* FirebaseAnalytics_KnownWindowsDllHashes[] = {
    "9d31987cb2d77f3808edc1705537357ee74e6d6be286eaf41a7e83cf82a6a7ba",
    "c49ec57e6f62ab6468e211c95e600a3df15cd8744a7cfc122b13c497558d0894",
    "449a1dcb57cc3db3c29f2c9e3b0b563a6654d0c66381c2c8fb62203f2f74e2a3"
};

// Count of known Google Analytics Windows DLL SHA256 hashes.
const int FirebaseAnalytics_KnownWindowsDllHashCount = 3;
#endif  // defined(_WIN32)

// --- Stub Function Definitions ---
// Stub for MyTestFunction1
static void Stub_MyTestFunction1(int param) {
    // No return value.
}

// Stub for MyTestFunction2
static bool Stub_MyTestFunction2() {
    return 1;
}


// --- Function Pointer Initializations ---
void (*ptr_MyTestFunction1)(int param) = &Stub_MyTestFunction1;
bool (*ptr_MyTestFunction2)() = &Stub_MyTestFunction2;

// --- Dynamic Loader Function for Windows ---
#if defined(_WIN32)
int FirebaseAnalytics_LoadDynamicFunctions(HMODULE dll_handle) {
    int count = 0;

    if (!dll_handle) {
        return count;
    }

    FARPROC proc_MyTestFunction1 = GetProcAddress(dll_handle, "MyTestFunction1");
    if (proc_MyTestFunction1) {
        ptr_MyTestFunction1 = (void (*)(int param))proc_MyTestFunction1;
        count++;
    }
    FARPROC proc_MyTestFunction2 = GetProcAddress(dll_handle, "MyTestFunction2");
    if (proc_MyTestFunction2) {
        ptr_MyTestFunction2 = (bool (*)())proc_MyTestFunction2;
        count++;
    }

    return count;
}

void FirebaseAnalytics_UnloadDynamicFunctions(void) {
    ptr_MyTestFunction1 = &Stub_MyTestFunction1;
    ptr_MyTestFunction2 = &Stub_MyTestFunction2;
}

#endif // defined(_WIN32)
// clang-format on
