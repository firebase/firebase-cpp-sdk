#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "analytics/src/analytics_common.h"
#include "analytics/src/analytics_desktop_dynamic.h"
#include "analytics/src/include/firebase/analytics.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "firebase/log.h"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "analytics_windows.h"
#endif
namespace firebase {
namespace analytics {
#if defined(_WIN32)
#define ANALYTICS_DLL_FILENAME L"google_analytics.dll"
static HMODULE g_analytics_module = 0;
#endif
static bool g_initialized = false;
static int g_fake_instance_id = 0;
static bool g_analytics_collection_enabled = true;
namespace {
LogCallback& GetLogCallback() {
static LogCallback s_log_callback;
return s_log_callback;
}
std::mutex& GetLogCallbackMutex() {
static std::mutex s_log_callback_mutex;
return s_log_callback_mutex;
}
#if defined(_WIN32)
std::string& GetAppId() {
static std::string s_app_id;
return s_app_id;
}
std::string& GetPackageName() {
static std::string s_package_name;
return s_package_name;
}
#endif
}
LogLevel ConvertAnalyticsLogLevelToFirebaseLogLevel(GoogleAnalytics_LogLevel log_level) {
switch (log_level) {
case GoogleAnalytics_LogLevel_kDebug:
return kLogLevelDebug;
case GoogleAnalytics_LogLevel_kInfo:
return kLogLevelInfo;
case GoogleAnalytics_LogLevel_kWarning:
return kLogLevelWarning;
case GoogleAnalytics_LogLevel_kError:
return kLogLevelError;
default:
return kLogLevelInfo;
}
}
void GoogleAnalyticsWrapperLogCallback(int32_t log_level, const char* message) {
LogCallback callback_to_call;
{
std::lock_guard lock(GetLogCallbackMutex());
callback_to_call = GetLogCallback();
}
if (callback_to_call) {
LogLevel firebase_log_level = ConvertAnalyticsLogLevelToFirebaseLogLevel(
static_cast<GoogleAnalytics_LogLevel>(log_level));
callback_to_call(firebase_log_level, message ? message : "");
}
}
void Initialize(const App& app) {
{
std::lock_guard lock(internal::FutureData::mutex());
if (g_initialized) {
}
g_initialized = true;
internal::FutureData::Create();
g_fake_instance_id = 0;
bool has_log_callback = false;
{
std::lock_guard callback_lock(GetLogCallbackMutex());
has_log_callback = static_cast<bool>(GetLogCallback());
}
#if defined(_WIN32)
if (!g_analytics_module) {
std::vector<std::string> allowed_hashes;
for (int i = 0; i < FirebaseAnalytics_KnownWindowsDllHashCount; i++) {
allowed_hashes.push_back(std::string(FirebaseAnalytics_KnownWindowsDllHashes[i]));
}
g_analytics_module = firebase::analytics::internal::VerifyAndLoadAnalyticsLibrary(ANALYTICS_DLL_FILENAME,
allowed_hashes);
if (g_analytics_module) {
int num_loaded = FirebaseAnalytics_LoadDynamicFunctions(g_analytics_module);
if (num_loaded < FirebaseAnalytics_DynamicFunctionCount) {
FirebaseAnalytics_UnloadDynamicFunctions();
FreeLibrary(g_analytics_module);
g_analytics_module = 0;
} else {
LogInfo("Analytics: Loaded Google Analytics module.");
GoogleAnalytics_SetLogCallback(GoogleAnalyticsWrapperLogCallback);
std::string& app_id = GetAppId();
std::string& package_name = GetPackageName();
app_id = app.options().app_id();
package_name = app.options().package_name();
GoogleAnalytics_Options* c_options = GoogleAnalytics_Options_Create();
if (!c_options) {
c_options->package_name = package_name.c_str();
c_options->analytics_collection_enabled_at_first_launch = g_analytics_collection_enabled;
LogInfo(
"Analytics: Initializing Google Analytics C API with App ID: "
"%s, Package Name: %s",
c_options->app_id, c_options->package_name);
if (!GoogleAnalytics_Initialize(c_options)) {
}
}
}
} else {
g_analytics_module = 0;
}
}
#endif
if (has_log_callback) { GoogleAnalytics_SetLogCallback(GoogleAnalyticsWrapperLogCallback); }
}
internal::RegisterTerminateOnAppDestroy(&app);
}
namespace internal {
bool IsInitialized() {
std::lock_guard lock(FutureData::mutex());
return g_initialized;
}
}
void Terminate() {
{
std::lock_guard lock(internal::FutureData::mutex());
if (!g_initialized) { return; } #if defined(_WIN32)
if (g_analytics_module) {
NotifyAppLifecycleChange(AppLifecycleState::kTermination);
GoogleAnalytics_SetLogCallback(nullptr);
FirebaseAnalytics_UnloadDynamicFunctions();
FreeLibrary(g_analytics_module);
g_analytics_module = 0;
}
#endif
GoogleAnalytics_SetLogCallback(nullptr);
{
std::lock_guard callback_lock(GetLogCallbackMutex());
GetLogCallback() = nullptr;
}
internal::FutureData::Destroy();
g_initialized = false;
}
internal::UnregisterTerminateOnAppDestroy();
}
static bool InsertVariantIntoGAItem(GoogleAnalytics_Item* c_item, const char* key, const firebase::Variant& value,
const char* param_name, const char* container_kind) {
if (value.is_int64()) {
GoogleAnalytics_Item_InsertInt(c_item, key, value.int64_value());
return true;
} else if (value.is_bool()) {
GoogleAnalytics_Item_InsertInt(c_item, key, value.bool_value() ? 1 : 0);
return true;
} else if (value.is_double()) {
GoogleAnalytics_Item_InsertDouble(c_item, key, value.double_value());
return true;
} else if (value.is_string()) {
GoogleAnalytics_Item_InsertString(c_item, key, value.string_value());
return true;
}
return false;
}
static void ConvertParametersToGAParams(const Parameter* parameters, size_t number_of_parameters,
GoogleAnalytics_EventParameters* c_event_params) {
if (!parameters || number_of_parameters == 0 || !c_event_params) { return; } for (size_t i = 0; i < number_of_parameters; ++i) {
const Parameter& param = parameters[i];
if (param.name == nullptr || param.name[0] == '\0') {
}
if (param.value.is_int64()) {
GoogleAnalytics_EventParameters_InsertInt(c_event_params, param.name, param.value.int64_value());
} else if (param.value.is_bool()) {
GoogleAnalytics_EventParameters_InsertInt(c_event_params, param.name, param.value.bool_value() ? 1 : 0);
} else if (param.value.is_double()) {
GoogleAnalytics_EventParameters_InsertDouble(c_event_params, param.name, param.value.double_value());
} else if (param.value.is_string()) {
GoogleAnalytics_EventParameters_InsertString(c_event_params, param.name, param.value.string_value());
} else if (param.value.is_vector()) {
const std::vector<firebase::Variant>& item_vector = param.value.vector();
if (item_vector.empty()) {
}
GoogleAnalytics_ItemVector* c_item_vector = GoogleAnalytics_ItemVector_Create();
if (!c_item_vector) {
}
bool item_vector_populated = false;
for (const firebase::Variant& element : item_vector) {
if (!element.is_map()) {
continue;
}
const std::map<firebase::Variant, firebase::Variant>& item_map = element.map();
if (item_map.empty()) { continue; } GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
if (!c_item) {
continue;
}
bool item_populated = false;
for (const auto& [key_variant, value_variant] : item_map) {
if (!key_variant.is_string()) {
}
const char* key_from_map = key_variant.string_value();
if (key_from_map == nullptr || key_from_map[0] == '\0') {
}
if (InsertVariantIntoGAItem(c_item, key_from_map, value_variant, param.name, "vector item")) {
item_populated = true;
}
}
if (item_populated) {
GoogleAnalytics_ItemVector_InsertItem(c_item_vector, c_item);
item_vector_populated = true;
} else {
GoogleAnalytics_Item_Destroy(c_item);
}
}
if (item_vector_populated) {
GoogleAnalytics_EventParameters_InsertItemVector(c_event_params, param.name, c_item_vector);
} else {
GoogleAnalytics_ItemVector_Destroy(c_item_vector);
"skipping.",
param.name);
}
} else if (param.value.is_map()) {
const std::map<firebase::Variant, firebase::Variant>& user_map = param.value.map();
if (user_map.empty()) {
}
GoogleAnalytics_ItemVector* c_item_vector = GoogleAnalytics_ItemVector_Create();
if (!c_item_vector) {
}
bool item_vector_populated = false;
for (const auto& [key_variant, value_variant] : user_map) {
if (!key_variant.is_string()) {
}
const char* key_from_map = key_variant.string_value();
if (key_from_map == nullptr || key_from_map[0] == '\0') {
}
GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
if (!c_item) {
continue;
}
if (InsertVariantIntoGAItem(c_item, key_from_map, value_variant, param.name, "map")) {
GoogleAnalytics_ItemVector_InsertItem(c_item_vector, c_item);
item_vector_populated = true;
} else {
GoogleAnalytics_Item_Destroy(c_item);
}
}
if (item_vector_populated) {
GoogleAnalytics_EventParameters_InsertItemVector(c_event_params, param.name, c_item_vector);
} else {
GoogleAnalytics_ItemVector_Destroy(c_item_vector);
"valid key-value pairs found or all values had unsupported types. "
"This map parameter was skipped.",
param.name);
}
} else {
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
if (name == nullptr || name[0] == '\0') {
}
if (parameters == nullptr && number_of_parameters > 0) {
return;
}
GoogleAnalytics_EventParameters* c_event_params = nullptr;
if (parameters != nullptr && number_of_parameters > 0) {
c_event_params = GoogleAnalytics_EventParameters_Create();
if (!c_event_params) {
}
ConvertParametersToGAParams(parameters, number_of_parameters, c_event_params);
}
GoogleAnalytics_LogEvent(name, c_event_params);
}
void LogEvent(const char* name, const std::vector<Parameter>& parameters) {
LogEvent(name, parameters.data(), parameters.size());
}
void SetUserProperty(const char* name, const char* property) {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
if (name == nullptr || name[0] == '\0') {
}
GoogleAnalytics_SetUserProperty(name, property);
}
void SetUserId(const char* user_id) {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
GoogleAnalytics_SetUserId(user_id);
}
void SetAnalyticsCollectionEnabled(bool enabled) {
std::lock_guard lock(internal::FutureData::mutex());
g_analytics_collection_enabled = enabled;
if (g_initialized) { GoogleAnalytics_SetAnalyticsCollectionEnabled(enabled); }
}
void ResetAnalyticsData() {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
GoogleAnalytics_ResetAnalyticsData();
g_fake_instance_id++;
}
void SetLogCallback(const LogCallback& callback) {
std::lock_guard lock(internal::FutureData::mutex());
{
std::lock_guard callback_lock(GetLogCallbackMutex());
GetLogCallback() = callback;
}
if (g_initialized) {
if (callback) {
GoogleAnalytics_SetLogCallback(GoogleAnalyticsWrapperLogCallback);
} else {
GoogleAnalytics_SetLogCallback(nullptr);
}
}
}
void NotifyAppLifecycleChange(AppLifecycleState state) {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
GoogleAnalytics_NotifyAppLifecycleChange(static_cast<GoogleAnalytics_AppLifecycleState>(state));
}
bool IsDesktopInitialized() {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
return GoogleAnalytics_IsInitialized();
}
void SetDesktopDebugMode(bool enabled) {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
GoogleAnalytics_SetDebugMode(enabled);
}
void LogEvent(const char* name) { LogEvent(name, static_cast<const Parameter*>(nullptr), 0); }
Future<void> LogAppleTransaction(const char* transaction_id) {
(void)transaction_id;
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
if (!future_data) { return Future<void>(); } auto* api = future_data->api();
const auto future_handle = api->SafeAlloc<void>(internal::kAnalyticsFnLogAppleTransaction);
api->Complete(future_handle, kErrorNone, "");
return Future<void>(api, future_handle.get());
}
Future<void> LogAppleTransactionLastResult() {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
if (!future_data) { return Future<void>(); } return static_cast<const Future<void>&>(future_data->api()->LastResult(internal::kAnalyticsFnLogAppleTransaction));
}
template <typename T>
void LogEventSingle(const char* name, const char* p_name, const T p_val) {
if (!p_name) {
}
Parameter param(p_name, p_val);
LogEvent(name, &param, 1);
}
void LogEvent(const char* name, const char* p_name, const char* p_val) {
LogEventSingle(name, p_name, p_val ? p_val : "");
}
void LogEvent(const char* name, const char* p_name, const double p_val) { LogEventSingle(name, p_name, p_val); }
void LogEvent(const char* name, const char* p_name, const int64_t p_val) { LogEventSingle(name, p_name, p_val); }
void LogEvent(const char* name, const char* p_name, const int p_val) {
LogEventSingle(name, p_name, static_cast<int64_t>(p_val));
}
Parameter param(parameter_name, static_cast<int64_t>(parameter_value));
LogEvent(name, &param, 1);
}
void SetDefaultEventParameters(const Parameter* parameters, size_t number_of_parameters) {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
if (parameters == nullptr && number_of_parameters > 0) {
return;
}
#if defined(_WIN32)
if (g_analytics_module) {
}
#endif
}
void SetDefaultEventParameters(const std::vector<Parameter>& parameters) {
SetDefaultEventParameters(parameters.data(), parameters.size());
}
void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
(void)consent_settings;
#if defined(_WIN32)
if (g_analytics_module) {
}
#endif
}
#define NOOP_CONVERSION_MEASUREMENT(func_name, type)                            \
void func_name(type arg) {                                                    \
(void)arg;                                                                  \
std::lock_guard lock(internal::FutureData::mutex());                        \
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());                     \
#if defined (_WIN32) if (g_analytics_module)                                \
FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
if (milliseconds < 0) {
GoogleAnalytics_SetSessionTimeoutInterval(-1);
return;
}
int64_t seconds = milliseconds / 1000;
if (milliseconds > 0 && seconds == 0) { seconds = 1; }
GoogleAnalytics_SetSessionTimeoutInterval(seconds);
}
template <typename T>
Future<T> CompleteStubFuture(internal::AnalyticsFn fn, T result_val, const char* name) {
std::lock_guard lock(internal::FutureData::mutex());
FIREBASE_ASSERT_RETURN(Future<T>(), internal::IsInitialized());
std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
if (!future_data) return Future<T>();
auto* api = future_data->api();
const auto future_handle = api->SafeAlloc<T>(fn);
api->CompleteWithResult(future_handle, kErrorNone, "", result_val);
#if defined(_WIN32)
}
template <typename T>
Future<T> GetStubLastResult(internal::AnalyticsFn fn, const char* name) {
#if defined(_WIN32)
}
Future<std::string> GetAnalyticsInstanceId() { return CompleteStubFuture<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId,
"FakeAnalyticsInstanceId" + std::to_string(g_fake_instance_id),
"GetAnalyticsInstanceId"); } Future<std::string> GetAnalyticsInstanceIdLastResult() { return GetStubLastResult<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId,
"GetAnalyticsInstanceIdLastResult"); } Future<int64_t> GetSessionId() { return CompleteStubFuture<int64_t>(internal::kAnalyticsFnGetSessionId, 0x5E5510171D570BL, "GetSessionId"); } Future<int64_t> GetSessionIdLastResult() { return GetStubLastResult<int64_t>(internal::kAnalyticsFnGetSessionId, "GetSessionIdLastResult"); } }
}