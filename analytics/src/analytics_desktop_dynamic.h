#ifndef FIREBASE_ANALYTICS_SRC_WINDOWS_ANALYTICS_DESKTOP_DYNAMIC_H_
#define FIREBASE_ANALYTICS_SRC_WINDOWS_ANALYTICS_DESKTOP_DYNAMIC_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifndef ANALYTICS_API
#define ANALYTICS_API
#define FIREBASE_ANALYTICS_UNDEF_ANALYTICS_API_
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct GoogleAnalytics_Reserved_Opaque GoogleAnalytics_Reserved;
typedef struct ANALYTICS_API GoogleAnalytics_Options {
  const char* app_id;
  const char* package_name;
  bool analytics_collection_enabled_at_first_launch;
  const char* app_data_directory;
  GoogleAnalytics_Reserved* reserved;
} GoogleAnalytics_Options;
typedef enum GoogleAnalytics_AppLifecycleState {
  GoogleAnalytics_AppLifecycleState_kUnknown = 0,
  GoogleAnalytics_AppLifecycleState_kTermination = 1,
} GoogleAnalytics_AppLifecycleState;
typedef enum GoogleAnalytics_LogLevel {
  GoogleAnalytics_LogLevel_kDebug = 0,
  GoogleAnalytics_LogLevel_kInfo = 1,
  GoogleAnalytics_LogLevel_kWarning = 2,
  GoogleAnalytics_LogLevel_kError = 3,
} GoogleAnalytics_LogLevel;
typedef void (*GoogleAnalytics_LogCallback)(int32_t log_level,
                                            const char* message);
typedef struct GoogleAnalytics_Item_Opaque GoogleAnalytics_Item;
typedef struct GoogleAnalytics_ItemVector_Opaque GoogleAnalytics_ItemVector;
typedef struct GoogleAnalytics_EventParameters_Opaque
    GoogleAnalytics_EventParameters;
extern GoogleAnalytics_Options* (*ptr_GoogleAnalytics_Options_Create)(void);
extern void (*ptr_GoogleAnalytics_Options_Destroy)(GoogleAnalytics_Options* options);
extern GoogleAnalytics_Item* (*ptr_GoogleAnalytics_Item_Create)(void);
extern bool (*ptr_GoogleAnalytics_Item_InsertInt)(GoogleAnalytics_Item* item, const char* key, int64_t value);
extern bool (*ptr_GoogleAnalytics_Item_InsertDouble)(GoogleAnalytics_Item* item, const char* key, double value);
extern bool (*ptr_GoogleAnalytics_Item_InsertString)(GoogleAnalytics_Item* item, const char* key, const char* value);
extern void (*ptr_GoogleAnalytics_Item_Destroy)(GoogleAnalytics_Item* item);
extern GoogleAnalytics_ItemVector* (*ptr_GoogleAnalytics_ItemVector_Create)(void);
extern bool (*ptr_GoogleAnalytics_ItemVector_InsertItem)(GoogleAnalytics_ItemVector* item_vector, GoogleAnalytics_Item* item);
extern void (*ptr_GoogleAnalytics_ItemVector_Destroy)(GoogleAnalytics_ItemVector* item_vector);
extern GoogleAnalytics_EventParameters* (*ptr_GoogleAnalytics_EventParameters_Create)(void);
extern bool (*ptr_GoogleAnalytics_EventParameters_InsertInt)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, int64_t value);
extern bool (*ptr_GoogleAnalytics_EventParameters_InsertDouble)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, double value);
extern bool (*ptr_GoogleAnalytics_EventParameters_InsertString)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, const char* value);
extern bool (*ptr_GoogleAnalytics_EventParameters_InsertItemVector)(GoogleAnalytics_EventParameters* event_parameter_map, const char* key, GoogleAnalytics_ItemVector* value);
extern void (*ptr_GoogleAnalytics_EventParameters_Destroy)(GoogleAnalytics_EventParameters* event_parameter_map);
extern bool (*ptr_GoogleAnalytics_Initialize)(GoogleAnalytics_Options* options);
extern void (*ptr_GoogleAnalytics_LogEvent)(const char* name, GoogleAnalytics_EventParameters* parameters);
extern void (*ptr_GoogleAnalytics_SetDefaultEventParameters)(GoogleAnalytics_EventParameters* parameters, size_t number_of_parameters);
extern void (*ptr_GoogleAnalytics_SetUserProperty)(const char* name, const char* value);
extern void (*ptr_GoogleAnalytics_SetUserId)(const char* user_id);
extern void (*ptr_GoogleAnalytics_ResetAnalyticsData)(void);
extern void (*ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled)(bool enabled);
extern void (*ptr_GoogleAnalytics_SetLogCallback)(GoogleAnalytics_LogCallback callback);
extern void (*ptr_GoogleAnalytics_SetSessionTimeoutInterval)(int64_t session_timeout_duration_seconds);
extern void (*ptr_GoogleAnalytics_NotifyAppLifecycleChange)(GoogleAnalytics_AppLifecycleState state);
extern bool (*ptr_GoogleAnalytics_IsInitialized)(void);
extern void (*ptr_GoogleAnalytics_SetDebugMode)(bool enabled);
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
#define GoogleAnalytics_SetDefaultEventParameters ptr_GoogleAnalytics_SetDefaultEventParameters
#define GoogleAnalytics_SetUserProperty ptr_GoogleAnalytics_SetUserProperty
#define GoogleAnalytics_SetUserId ptr_GoogleAnalytics_SetUserId
#define GoogleAnalytics_ResetAnalyticsData ptr_GoogleAnalytics_ResetAnalyticsData
#define GoogleAnalytics_SetAnalyticsCollectionEnabled ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled
#define GoogleAnalytics_SetLogCallback ptr_GoogleAnalytics_SetLogCallback
#define GoogleAnalytics_SetSessionTimeoutInterval ptr_GoogleAnalytics_SetSessionTimeoutInterval
#define GoogleAnalytics_NotifyAppLifecycleChange ptr_GoogleAnalytics_NotifyAppLifecycleChange
#define GoogleAnalytics_IsInitialized ptr_GoogleAnalytics_IsInitialized
#define GoogleAnalytics_SetDebugMode ptr_GoogleAnalytics_SetDebugMode
extern const int FirebaseAnalytics_DynamicFunctionCount;
#if defined(_WIN32)
extern const char* FirebaseAnalytics_KnownWindowsDllHashes[];
extern const int FirebaseAnalytics_KnownWindowsDllHashCount;
int FirebaseAnalytics_LoadDynamicFunctions(HMODULE dll_handle);
void FirebaseAnalytics_UnloadDynamicFunctions(void);
#endif
#ifdef __cplusplus
}
#endif
#ifdef FIREBASE_ANALYTICS_UNDEF_ANALYTICS_API_
#undef ANALYTICS_API
#undef FIREBASE_ANALYTICS_UNDEF_ANALYTICS_API_
#endif
#endif
