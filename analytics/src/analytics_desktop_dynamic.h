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

#define ANALYTICS_API  // filter out from header copy

#include <stdbool.h>  // needed for bool type in pure C

// --- Copied from original header ---
#include <stdbool.h>
#include <stdint.h>

typedef struct GoogleAnalytics_Reserved_Opaque GoogleAnalytics_Reserved;

/**
 * @brief GoogleAnalytics_Options for initializing the Analytics SDK.
 * GoogleAnalytics_Options_Create() must be used to create an instance of this
 * struct with default values. If these options are created manually instead of
 * using GoogleAnalytics_Options_Create(), initialization will fail, and the
 * caller will be responsible for destroying the options.
 */
ANALYTICS_API typedef struct {
  /**
   * @brief The unique identifier for the Firebase app across all of Firebase
   * with a platform-specific format. This is a required field, can not be null
   * or empty, and must be UTF-8 encoded.
   *
   * The caller is responsible for allocating this memory, and deallocating it
   * once the options instance has been destroyed.
   *
   * Example: 1:1234567890:android:321abc456def7890
   */
  const char* app_id;

  /**
   * @brief Unique identifier for the application implementing the SDK. The
   * format typically follows a reversed domain name convention. This is a
   * required field, can not be null or empty, and must be UTF-8 encoded.
   *
   * The caller is responsible for allocating this memory, and deallocating it
   * once the options instance has been destroyed.
   *
   * Example: com.google.analytics.AnalyticsApp
   */
  const char* package_name;

  /**
   * @brief Whether Analytics is enabled at the very first launch.
   * This value is then persisted across app sessions, and from then on, takes
   * precedence over the value of this field.
   * GoogleAnalytics_SetAnalyticsCollectionEnabled() can be used to
   * enable/disable after that point.
   */
  bool analytics_collection_enabled_at_first_launch;

  /**
   * @brief Reserved for internal use by the SDK.
   */
  GoogleAnalytics_Reserved* reserved;
} GoogleAnalytics_Options;

/**
 * @brief Creates an instance of GoogleAnalytics_Options with default values.
 *
 * The caller is responsible for destroying the options using the
 * GoogleAnalytics_Options_Destroy() function, unless it has been passed to the
 * GoogleAnalytics_Initialize() function, in which case it will be destroyed
 * automatically.
 *
 * @return A pointer to a newly allocated GoogleAnalytics_Options instance.
 */
ANALYTICS_API GoogleAnalytics_Options* GoogleAnalytics_Options_Create();

/**
 * @brief Destroys the GoogleAnalytics_Options instance. Must not be called if
 * the options were created with GoogleAnalytics_Options_Create() and passed to
 * the GoogleAnalytics_Initialize() function, which would destroy them
 * automatically.
 *
 * @param[in] options The GoogleAnalytics_Options instance to destroy.
 */
ANALYTICS_API void GoogleAnalytics_Options_Destroy(
    GoogleAnalytics_Options* options);

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
// clang-format off
extern GoogleAnalytics_Options* (*ptr_GoogleAnalytics_Options_Create)();
extern void (*ptr_GoogleAnalytics_Options_Destroy)(GoogleAnalytics_Options* options);
extern GoogleAnalytics_Item* (*ptr_GoogleAnalytics_Item_Create)();
extern void (*ptr_GoogleAnalytics_Item_InsertInt)(GoogleAnalytics_Item* item, const char* key, int64_t value);
extern void (*ptr_GoogleAnalytics_Item_InsertDouble)(GoogleAnalytics_Item* item, const char* key, double value);
extern void (*ptr_GoogleAnalytics_Item_InsertString)(GoogleAnalytics_Item* item, const char* key, const char* value);
extern void (*ptr_GoogleAnalytics_Item_Destroy)(GoogleAnalytics_Item* item);
extern GoogleAnalytics_ItemVector* (*ptr_GoogleAnalytics_ItemVector_Create)();
extern void (*ptr_GoogleAnalytics_ItemVector_InsertItem)(GoogleAnalytics_ItemVector* item_vector, GoogleAnalytics_Item* item);
extern void (*ptr_GoogleAnalytics_ItemVector_Destroy)(GoogleAnalytics_ItemVector* item_vector);
extern GoogleAnalytics_EventParameters* (*ptr_GoogleAnalytics_EventParameters_Create)();
extern void (*ptr_GoogleAnalytics_EventParameters_InsertInt)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, int64_t value);
extern void (*ptr_GoogleAnalytics_EventParameters_InsertDouble)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, double value);
extern void (*ptr_GoogleAnalytics_EventParameters_InsertString)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, const char* value);
extern void (*ptr_GoogleAnalytics_EventParameters_InsertItemVector)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, GoogleAnalytics_ItemVector* value);
extern void (*ptr_GoogleAnalytics_EventParameters_Destroy)(GoogleAnalytics_EventParameters* event_parameter_map);
extern bool (*ptr_GoogleAnalytics_Initialize)(const GoogleAnalytics_Options* options);
extern void (*ptr_GoogleAnalytics_LogEvent)(const char* name, GoogleAnalytics_EventParameters* parameters);
extern void (*ptr_GoogleAnalytics_SetUserProperty)(const char* name, const char* value);
extern void (*ptr_GoogleAnalytics_SetUserId)(const char* user_id);
extern void (*ptr_GoogleAnalytics_ResetAnalyticsData)();
extern void (*ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled)(bool enabled);

#define GoogleAnalytics_Options_Create ptr_GoogleAnalytics_Options_Create
#define GoogleAnalytics_Options_Destroy ptr_GoogleAnalytics_Options_Destroy
#define GoogleAnalytics_Item_Create ptr_GoogleAnalytics_Item_Create
#define GoogleAnalytics_Item_InsertInt ptr_GoogleAnalytics_Item_InsertInt
#define GoogleAnalytics_Item_InsertDouble ptr_GoogleAnalytics_Item_InsertDouble
#define GoogleAnalytics_Item_InsertString ptr_GoogleAnalytics_Item_InsertString
#define GoogleAnalytics_Item_Destroy ptr_GoogleAnalytics_Item_Destroy
#define GoogleAnalytics_ItemVector_Create ptr_GoogleAnalytics_ItemVector_Create
#define GoogleAnalytics_ItemVector_InsertItem ptr_GoogleAnalytics_ItemVector_InsertItem
#define GoogleAnalytics_ItemVector_Destroy ptr_GoogleAnalytics_ItemVector_Destroy
#define GoogleAnalytics_EventParameters_Create ptr_GoogleAnalytics_EventParameters_Create
#define GoogleAnalytics_EventParameters_InsertInt ptr_GoogleAnalytics_EventParameters_InsertInt
#define GoogleAnalytics_EventParameters_InsertDouble ptr_GoogleAnalytics_EventParameters_InsertDouble
#define GoogleAnalytics_EventParameters_InsertString ptr_GoogleAnalytics_EventParameters_InsertString
#define GoogleAnalytics_EventParameters_InsertItemVector ptr_GoogleAnalytics_EventParameters_InsertItemVector
#define GoogleAnalytics_EventParameters_Destroy ptr_GoogleAnalytics_EventParameters_Destroy
#define GoogleAnalytics_Initialize ptr_GoogleAnalytics_Initialize
#define GoogleAnalytics_LogEvent ptr_GoogleAnalytics_LogEvent
#define GoogleAnalytics_SetUserProperty ptr_GoogleAnalytics_SetUserProperty
#define GoogleAnalytics_SetUserId ptr_GoogleAnalytics_SetUserId
#define GoogleAnalytics_ResetAnalyticsData ptr_GoogleAnalytics_ResetAnalyticsData
#define GoogleAnalytics_SetAnalyticsCollectionEnabled ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled
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
