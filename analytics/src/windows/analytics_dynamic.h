// Generated from analytics_windows.h

#ifndef FIREBASE_ANALYTICS_ANALYTICS_DYNAMIC_H_
#define FIREBASE_ANALYTICS_ANALYTICS_DYNAMIC_H_

#include "analytics/src/windowsanalytics_windows.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Function Pointer Declarations ---
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
extern void (*ptr_GoogleAnalytics_LogEvent)(const char* name, GoogleAnalytics_EventParameters* parameters);
extern void (*ptr_GoogleAnalytics_SetUserProperty)(const char* name, const char* value);
extern void (*ptr_GoogleAnalytics_SetUserId)(const char* user_id);
extern void (*ptr_GoogleAnalytics_ResetAnalyticsData)();
extern void (*ptr_GoogleAnalytics_SetAnalyticsCollectionEnabled)(bool enabled);

// --- Dynamic Loader Declaration for Windows ---
#if defined(_WIN32)
#include <windows.h> // For HMODULE
void LoadAnalyticsFunctions(HMODULE dll_handle);
#endif // defined(_WIN32)

#ifdef __cplusplus
}
#endif

#endif  // FIREBASE_ANALYTICS_ANALYTICS_DYNAMIC_H_
