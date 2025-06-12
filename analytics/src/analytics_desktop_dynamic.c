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

// clang-format off

static void* g_stub_memory = NULL;

// --- Stub Function Definitions ---
// Stub for GoogleAnalytics_Item_Create
static GoogleAnalytics_Item* Stub_GoogleAnalytics_Item_Create() {
    return (GoogleAnalytics_Item*)(&g_stub_memory);
}

// Stub for GoogleAnalytics_Item_InsertInt
static void Stub_GoogleAnalytics_Item_InsertInt(GoogleAnalytics_Item* item,
                                                  const char* key,
                                                  int64_t value) {
    // No return value.
}

// Stub for GoogleAnalytics_Item_InsertDouble
static void Stub_GoogleAnalytics_Item_InsertDouble(GoogleAnalytics_Item* item,
                                                     const char* key,
                                                     double value) {
    // No return value.
}

// Stub for GoogleAnalytics_Item_InsertString
static void Stub_GoogleAnalytics_Item_InsertString(GoogleAnalytics_Item* item,
                                                     const char* key,
                                                     const char* value) {
    // No return value.
}

// Stub for GoogleAnalytics_Item_Destroy
static void Stub_GoogleAnalytics_Item_Destroy(GoogleAnalytics_Item* item) {
    // No return value.
}

// Stub for GoogleAnalytics_ItemVector_Create
static GoogleAnalytics_ItemVector* Stub_GoogleAnalytics_ItemVector_Create() {
    return (GoogleAnalytics_ItemVector*)(&g_stub_memory);
}

// Stub for GoogleAnalytics_ItemVector_InsertItem
static void Stub_GoogleAnalytics_ItemVector_InsertItem(GoogleAnalytics_ItemVector* item_vector, GoogleAnalytics_Item* item) {
    // No return value.
}

// Stub for GoogleAnalytics_ItemVector_Destroy
static void Stub_GoogleAnalytics_ItemVector_Destroy(GoogleAnalytics_ItemVector* item_vector) {
    // No return value.
}

// Stub for GoogleAnalytics_EventParameters_Create
static GoogleAnalytics_EventParameters* Stub_GoogleAnalytics_EventParameters_Create() {
    return (GoogleAnalytics_EventParameters*)(&g_stub_memory);
}

// Stub for GoogleAnalytics_EventParameters_InsertInt
static void Stub_GoogleAnalytics_EventParameters_InsertInt(GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    int64_t value) {
    // No return value.
}

// Stub for GoogleAnalytics_EventParameters_InsertDouble
static void Stub_GoogleAnalytics_EventParameters_InsertDouble(GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    double value) {
    // No return value.
}

// Stub for GoogleAnalytics_EventParameters_InsertString
static void Stub_GoogleAnalytics_EventParameters_InsertString(GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    const char* value) {
    // No return value.
}

// Stub for GoogleAnalytics_EventParameters_InsertItemVector
static void Stub_GoogleAnalytics_EventParameters_InsertItemVector(GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    GoogleAnalytics_ItemVector* value) {
    // No return value.
}

// Stub for GoogleAnalytics_EventParameters_Destroy
static void Stub_GoogleAnalytics_EventParameters_Destroy(GoogleAnalytics_EventParameters* event_parameter_map) {
    // No return value.
}

// Stub for GoogleAnalytics_LogEvent
static void Stub_GoogleAnalytics_LogEvent(const char* name, GoogleAnalytics_EventParameters* parameters) {
    // No return value.
}

// Stub for GoogleAnalytics_SetUserProperty
static void Stub_GoogleAnalytics_SetUserProperty(const char* name,
                                                   const char* value) {
    // No return value.
}

// Stub for GoogleAnalytics_SetUserId
static void Stub_GoogleAnalytics_SetUserId(const char* user_id) {
    // No return value.
}

// Stub for GoogleAnalytics_ResetAnalyticsData
static void Stub_GoogleAnalytics_ResetAnalyticsData() {
    // No return value.
}

// Stub for GoogleAnalytics_SetAnalyticsCollectionEnabled
static void Stub_GoogleAnalytics_SetAnalyticsCollectionEnabled(bool enabled) {
    // No return value.
}


// --- Function Pointer Initializations ---
GoogleAnalytics_Item* (*ptr_GoogleAnalytics_Item_Create)() = &Stub_GoogleAnalytics_Item_Create;
void (*ptr_GoogleAnalytics_Item_InsertInt)(GoogleAnalytics_Item* item, const char* key, int64_t value) = &Stub_GoogleAnalytics_Item_InsertInt;
void (*ptr_GoogleAnalytics_Item_InsertDouble)(GoogleAnalytics_Item* item, const char* key, double value) = &Stub_GoogleAnalytics_Item_InsertDouble;
void (*ptr_GoogleAnalytics_Item_InsertString)(GoogleAnalytics_Item* item, const char* key, const char* value) = &Stub_GoogleAnalytics_Item_InsertString;
void (*ptr_GoogleAnalytics_Item_Destroy)(GoogleAnalytics_Item* item) = &Stub_GoogleAnalytics_Item_Destroy;
GoogleAnalytics_ItemVector* (*ptr_GoogleAnalytics_ItemVector_Create)() = &Stub_GoogleAnalytics_ItemVector_Create;
void (*ptr_GoogleAnalytics_ItemVector_InsertItem)(GoogleAnalytics_ItemVector* item_vector, GoogleAnalytics_Item* item) = &Stub_GoogleAnalytics_ItemVector_InsertItem;
void (*ptr_GoogleAnalytics_ItemVector_Destroy)(GoogleAnalytics_ItemVector* item_vector) = &Stub_GoogleAnalytics_ItemVector_Destroy;
GoogleAnalytics_EventParameters* (*ptr_GoogleAnalytics_EventParameters_Create)() = &Stub_GoogleAnalytics_EventParameters_Create;
void (*ptr_GoogleAnalytics_EventParameters_InsertInt)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, int64_t value) = &Stub_GoogleAnalytics_EventParameters_InsertInt;
void (*ptr_GoogleAnalytics_EventParameters_InsertDouble)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, double value) = &Stub_GoogleAnalytics_EventParameters_InsertDouble;
void (*ptr_GoogleAnalytics_EventParameters_InsertString)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, const char* value) = &Stub_GoogleAnalytics_EventParameters_InsertString;
void (*ptr_GoogleAnalytics_EventParameters_InsertItemVector)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, GoogleAnalytics_ItemVector* value) = &Stub_GoogleAnalytics_EventParameters_InsertItemVector;
void (*ptr_GoogleAnalytics_EventParameters_Destroy)(GoogleAnalytics_EventParameters* event_parameter_map) = &Stub_GoogleAnalytics_EventParameters_Destroy;
void (*ptr_GoogleAnalytics_LogEvent)(const char* name, GoogleAnalytics_EventParameters* parameters) = &Stub_GoogleAnalytics_LogEvent;
void (*ptr_GoogleAnalytics_SetUserProperty)(const char* name, const char* value) = &Stub_GoogleAnalytics_SetUserProperty;
void (*ptr_GoogleAnalytics_SetUserId)(const char* user_id) = &Stub_GoogleAnalytics_SetUserId;
void (*ptr_GoogleAnalytics_ResetAnalyticsData)() = &Stub_GoogleAnalytics_ResetAnalyticsData;
void (*ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled)(bool enabled) = &Stub_GoogleAnalytics_SetAnalyticsCollectionEnabled;

// --- Dynamic Loader Function for Windows ---
#if defined(_WIN32)
int FirebaseAnalytics_LoadAnalyticsFunctions(HMODULE dll_handle) {
    int count = 0;

    if (!dll_handle) {
        return count;
    }

    FARPROC proc_GoogleAnalytics_Item_Create = GetProcAddress(dll_handle, "GoogleAnalytics_Item_Create");
    if (proc_GoogleAnalytics_Item_Create) {
        ptr_GoogleAnalytics_Item_Create = (GoogleAnalytics_Item* (*)())proc_GoogleAnalytics_Item_Create;
        count++;
    }
    FARPROC proc_GoogleAnalytics_Item_InsertInt = GetProcAddress(dll_handle, "GoogleAnalytics_Item_InsertInt");
    if (proc_GoogleAnalytics_Item_InsertInt) {
        ptr_GoogleAnalytics_Item_InsertInt = (void (*)(GoogleAnalytics_Item* item, const char* key, int64_t value))proc_GoogleAnalytics_Item_InsertInt;
        count++;
    }
    FARPROC proc_GoogleAnalytics_Item_InsertDouble = GetProcAddress(dll_handle, "GoogleAnalytics_Item_InsertDouble");
    if (proc_GoogleAnalytics_Item_InsertDouble) {
        ptr_GoogleAnalytics_Item_InsertDouble = (void (*)(GoogleAnalytics_Item* item, const char* key, double value))proc_GoogleAnalytics_Item_InsertDouble;
        count++;
    }
    FARPROC proc_GoogleAnalytics_Item_InsertString = GetProcAddress(dll_handle, "GoogleAnalytics_Item_InsertString");
    if (proc_GoogleAnalytics_Item_InsertString) {
        ptr_GoogleAnalytics_Item_InsertString = (void (*)(GoogleAnalytics_Item* item, const char* key, const char* value))proc_GoogleAnalytics_Item_InsertString;
        count++;
    }
    FARPROC proc_GoogleAnalytics_Item_Destroy = GetProcAddress(dll_handle, "GoogleAnalytics_Item_Destroy");
    if (proc_GoogleAnalytics_Item_Destroy) {
        ptr_GoogleAnalytics_Item_Destroy = (void (*)(GoogleAnalytics_Item* item))proc_GoogleAnalytics_Item_Destroy;
        count++;
    }
    FARPROC proc_GoogleAnalytics_ItemVector_Create = GetProcAddress(dll_handle, "GoogleAnalytics_ItemVector_Create");
    if (proc_GoogleAnalytics_ItemVector_Create) {
        ptr_GoogleAnalytics_ItemVector_Create = (GoogleAnalytics_ItemVector* (*)())proc_GoogleAnalytics_ItemVector_Create;
        count++;
    }
    FARPROC proc_GoogleAnalytics_ItemVector_InsertItem = GetProcAddress(dll_handle, "GoogleAnalytics_ItemVector_InsertItem");
    if (proc_GoogleAnalytics_ItemVector_InsertItem) {
        ptr_GoogleAnalytics_ItemVector_InsertItem = (void (*)(GoogleAnalytics_ItemVector* item_vector, GoogleAnalytics_Item* item))proc_GoogleAnalytics_ItemVector_InsertItem;
        count++;
    }
    FARPROC proc_GoogleAnalytics_ItemVector_Destroy = GetProcAddress(dll_handle, "GoogleAnalytics_ItemVector_Destroy");
    if (proc_GoogleAnalytics_ItemVector_Destroy) {
        ptr_GoogleAnalytics_ItemVector_Destroy = (void (*)(GoogleAnalytics_ItemVector* item_vector))proc_GoogleAnalytics_ItemVector_Destroy;
        count++;
    }
    FARPROC proc_GoogleAnalytics_EventParameters_Create = GetProcAddress(dll_handle, "GoogleAnalytics_EventParameters_Create");
    if (proc_GoogleAnalytics_EventParameters_Create) {
        ptr_GoogleAnalytics_EventParameters_Create = (GoogleAnalytics_EventParameters* (*)())proc_GoogleAnalytics_EventParameters_Create;
        count++;
    }
    FARPROC proc_GoogleAnalytics_EventParameters_InsertInt = GetProcAddress(dll_handle, "GoogleAnalytics_EventParameters_InsertInt");
    if (proc_GoogleAnalytics_EventParameters_InsertInt) {
        ptr_GoogleAnalytics_EventParameters_InsertInt = (void (*)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, int64_t value))proc_GoogleAnalytics_EventParameters_InsertInt;
        count++;
    }
    FARPROC proc_GoogleAnalytics_EventParameters_InsertDouble = GetProcAddress(dll_handle, "GoogleAnalytics_EventParameters_InsertDouble");
    if (proc_GoogleAnalytics_EventParameters_InsertDouble) {
        ptr_GoogleAnalytics_EventParameters_InsertDouble = (void (*)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, double value))proc_GoogleAnalytics_EventParameters_InsertDouble;
        count++;
    }
    FARPROC proc_GoogleAnalytics_EventParameters_InsertString = GetProcAddress(dll_handle, "GoogleAnalytics_EventParameters_InsertString");
    if (proc_GoogleAnalytics_EventParameters_InsertString) {
        ptr_GoogleAnalytics_EventParameters_InsertString = (void (*)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, const char* value))proc_GoogleAnalytics_EventParameters_InsertString;
        count++;
    }
    FARPROC proc_GoogleAnalytics_EventParameters_InsertItemVector = GetProcAddress(dll_handle, "GoogleAnalytics_EventParameters_InsertItemVector");
    if (proc_GoogleAnalytics_EventParameters_InsertItemVector) {
        ptr_GoogleAnalytics_EventParameters_InsertItemVector = (void (*)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, GoogleAnalytics_ItemVector* value))proc_GoogleAnalytics_EventParameters_InsertItemVector;
        count++;
    }
    FARPROC proc_GoogleAnalytics_EventParameters_Destroy = GetProcAddress(dll_handle, "GoogleAnalytics_EventParameters_Destroy");
    if (proc_GoogleAnalytics_EventParameters_Destroy) {
        ptr_GoogleAnalytics_EventParameters_Destroy = (void (*)(GoogleAnalytics_EventParameters* event_parameter_map))proc_GoogleAnalytics_EventParameters_Destroy;
        count++;
    }
    FARPROC proc_GoogleAnalytics_LogEvent = GetProcAddress(dll_handle, "GoogleAnalytics_LogEvent");
    if (proc_GoogleAnalytics_LogEvent) {
        ptr_GoogleAnalytics_LogEvent = (void (*)(const char* name, GoogleAnalytics_EventParameters* parameters))proc_GoogleAnalytics_LogEvent;
        count++;
    }
    FARPROC proc_GoogleAnalytics_SetUserProperty = GetProcAddress(dll_handle, "GoogleAnalytics_SetUserProperty");
    if (proc_GoogleAnalytics_SetUserProperty) {
        ptr_GoogleAnalytics_SetUserProperty = (void (*)(const char* name, const char* value))proc_GoogleAnalytics_SetUserProperty;
        count++;
    }
    FARPROC proc_GoogleAnalytics_SetUserId = GetProcAddress(dll_handle, "GoogleAnalytics_SetUserId");
    if (proc_GoogleAnalytics_SetUserId) {
        ptr_GoogleAnalytics_SetUserId = (void (*)(const char* user_id))proc_GoogleAnalytics_SetUserId;
        count++;
    }
    FARPROC proc_GoogleAnalytics_ResetAnalyticsData = GetProcAddress(dll_handle, "GoogleAnalytics_ResetAnalyticsData");
    if (proc_GoogleAnalytics_ResetAnalyticsData) {
        ptr_GoogleAnalytics_ResetAnalyticsData = (void (*)())proc_GoogleAnalytics_ResetAnalyticsData;
        count++;
    }
    FARPROC proc_GoogleAnalytics_SetAnalyticsCollectionEnabled = GetProcAddress(dll_handle, "GoogleAnalytics_SetAnalyticsCollectionEnabled");
    if (proc_GoogleAnalytics_SetAnalyticsCollectionEnabled) {
        ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled = (void (*)(bool enabled))proc_GoogleAnalytics_SetAnalyticsCollectionEnabled;
        count++;
    }

    return count;
}

void FirebaseAnalytics_UnloadAnalyticsFunctions(void) {
    ptr_GoogleAnalytics_Item_Create = &Stub_GoogleAnalytics_Item_Create;
    ptr_GoogleAnalytics_Item_InsertInt = &Stub_GoogleAnalytics_Item_InsertInt;
    ptr_GoogleAnalytics_Item_InsertDouble = &Stub_GoogleAnalytics_Item_InsertDouble;
    ptr_GoogleAnalytics_Item_InsertString = &Stub_GoogleAnalytics_Item_InsertString;
    ptr_GoogleAnalytics_Item_Destroy = &Stub_GoogleAnalytics_Item_Destroy;
    ptr_GoogleAnalytics_ItemVector_Create = &Stub_GoogleAnalytics_ItemVector_Create;
    ptr_GoogleAnalytics_ItemVector_InsertItem = &Stub_GoogleAnalytics_ItemVector_InsertItem;
    ptr_GoogleAnalytics_ItemVector_Destroy = &Stub_GoogleAnalytics_ItemVector_Destroy;
    ptr_GoogleAnalytics_EventParameters_Create = &Stub_GoogleAnalytics_EventParameters_Create;
    ptr_GoogleAnalytics_EventParameters_InsertInt = &Stub_GoogleAnalytics_EventParameters_InsertInt;
    ptr_GoogleAnalytics_EventParameters_InsertDouble = &Stub_GoogleAnalytics_EventParameters_InsertDouble;
    ptr_GoogleAnalytics_EventParameters_InsertString = &Stub_GoogleAnalytics_EventParameters_InsertString;
    ptr_GoogleAnalytics_EventParameters_InsertItemVector = &Stub_GoogleAnalytics_EventParameters_InsertItemVector;
    ptr_GoogleAnalytics_EventParameters_Destroy = &Stub_GoogleAnalytics_EventParameters_Destroy;
    ptr_GoogleAnalytics_LogEvent = &Stub_GoogleAnalytics_LogEvent;
    ptr_GoogleAnalytics_SetUserProperty = &Stub_GoogleAnalytics_SetUserProperty;
    ptr_GoogleAnalytics_SetUserId = &Stub_GoogleAnalytics_SetUserId;
    ptr_GoogleAnalytics_ResetAnalyticsData = &Stub_GoogleAnalytics_ResetAnalyticsData;
    ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled = &Stub_GoogleAnalytics_SetAnalyticsCollectionEnabled;
}

#endif // defined(_WIN32)
// clang-format on
