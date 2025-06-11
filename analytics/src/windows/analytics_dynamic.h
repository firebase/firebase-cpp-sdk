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

// Generated from analytics.h
// This is a self-contained header file.

#ifndef FIREBASE_ANALYTICS_SRC_WINDOWS_ANALYTICS_DYNAMIC_H_
#define FIREBASE_ANALYTICS_SRC_WINDOWS_ANALYTICS_DYNAMIC_H_

// --- Copied from original header ---
#include <stdint.h>

/**
 * @brief Opaque type for an item.
 *
 * This type is an opaque object that represents an item in an item vector.
 *
 * The caller is responsible for creating the item using the
 * GoogleAnalytics_Item_Create() function, and destroying it using the
 * GoogleAnalytics_Item_Destroy() function, unless it has been added to an
 * item vector, in which case it will be destroyed at that time.
 */
typedef struct GoogleAnalytics_Item_Opaque GoogleAnalytics_Item;

/**
 * @brief Opaque type for an item vector.
 *
 * This type is an opaque object that represents a list of items. It is
 * used to pass item vectors to the
 * GoogleAnalytics_EventParameters_InsertItemVector() function.
 *
 * The caller is responsible for creating the item vector using the
 * GoogleAnalytics_ItemVector_Create() function, and destroying it using the
 * GoogleAnalytics_ItemVector_Destroy() function, unless it has been added
 * to an event parameter map, in which case it will be destroyed at that time.
 */
typedef struct GoogleAnalytics_ItemVector_Opaque GoogleAnalytics_ItemVector;

/**
 * @brief Opaque type for an event parameter map.
 *
 * This type is an opaque object that represents a dictionary of event
 * parameters. It is used to pass event parameters to the
 * GoogleAnalytics_LogEvent() function.
 *
 * The caller is responsible for creating the event parameter map using the
 * GoogleAnalytics_EventParameters_Create() function, and destroying it using
 * the GoogleAnalytics_EventParameters_Destroy() function, unless it has been
 * logged, in which case it will be destroyed automatically.
 */
typedef struct GoogleAnalytics_EventParameters_Opaque
    GoogleAnalytics_EventParameters;

// --- End of copied section ---

#ifdef __cplusplus
extern "C" {
#endif

// --- Function Pointer Declarations ---
extern GoogleAnalytics_Item* (*ptr_GoogleAnalytics_Item_Create)();
extern void (*ptr_GoogleAnalytics_Item_InsertInt)(GoogleAnalytics_Item* item,
                                                  const char* key,
                                                  int64_t value);
extern void (*ptr_GoogleAnalytics_Item_InsertDouble)(GoogleAnalytics_Item* item,
                                                     const char* key,
                                                     double value);
extern void (*ptr_GoogleAnalytics_Item_InsertString)(GoogleAnalytics_Item* item,
                                                     const char* key,
                                                     const char* value);
extern void (*ptr_GoogleAnalytics_Item_Destroy)(GoogleAnalytics_Item* item);
extern GoogleAnalytics_ItemVector* (*ptr_GoogleAnalytics_ItemVector_Create)();
extern void (*ptr_GoogleAnalytics_ItemVector_InsertItem)(
    GoogleAnalytics_ItemVector* item_vector, GoogleAnalytics_Item* item);
extern void (*ptr_GoogleAnalytics_ItemVector_Destroy)(
    GoogleAnalytics_ItemVector* item_vector);
extern GoogleAnalytics_EventParameters* (
    *ptr_GoogleAnalytics_EventParameters_Create)();
extern void (*ptr_GoogleAnalytics_EventParameters_InsertInt)(
    GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    int64_t value);
extern void (*ptr_GoogleAnalytics_EventParameters_InsertDouble)(
    GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    double value);
extern void (*ptr_GoogleAnalytics_EventParameters_InsertString)(
    GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    const char* value);
extern void (*ptr_GoogleAnalytics_EventParameters_InsertItemVector)(
    GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    GoogleAnalytics_ItemVector* value);
extern void (*ptr_GoogleAnalytics_EventParameters_Destroy)(
    GoogleAnalytics_EventParameters* event_parameter_map);
extern void (*ptr_GoogleAnalytics_LogEvent)(
    const char* name, GoogleAnalytics_EventParameters* parameters);
extern void (*ptr_GoogleAnalytics_SetUserProperty)(const char* name,
                                                   const char* value);
extern void (*ptr_GoogleAnalytics_SetUserId)(const char* user_id);
extern void (*ptr_GoogleAnalytics_ResetAnalyticsData)();
extern void (*ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled)(bool enabled);

// --- Dynamic Loader Declaration for Windows ---
#if defined(_WIN32)
#include <windows.h>  // For HMODULE
void FirebaseAnalytics_LoadAnalyticsFunctions(HMODULE dll_handle);
#endif  // defined(_WIN32)

#ifdef __cplusplus
}
#endif

#endif  // FIREBASE_ANALYTICS_SRC_WINDOWS_ANALYTICS_DYNAMIC_H_
