// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "testlab/src/common/common.h"

namespace firebase {
namespace test_lab {
namespace game_loop {
namespace internal {

static const ::firebase::App* g_app = nullptr;
static const char* kFirebaseTestLabAuthority =
    "content://com.google.firebase.testlab";
static const char* kScenarioCol = "scenario";
static const char* kCustomResultsCol = "customResultUri";

std::string* g_custom_result_uri;

int GetScenario();
static void InitFromIntent();
static bool InitFromContentProvider();
static void GetIntentUri();

FILE* GetTempFile();

void CreateOrOpenLogFile() {
  if (!g_log_file) {
    g_log_file = GetTempFile();
  }
  if (!g_log_file) {
    LogError(
        "Could not create a cache directory file for custom results. No custom "
        "results will "
        "be logged for the duration of the game loop scenario. %s",
        strerror(errno));
  }
}

void Initialize(const ::firebase::App* app) {
  g_app = app;
  CreateOrOpenLogFile();
  if (!InitFromContentProvider()) {
    LogDebug(
        "Could not find scenario data from content provider, falling back to "
        "intent");
    InitFromIntent();
  }
}

void Terminate() {
  g_app = nullptr;
  if (g_custom_result_uri) {
    delete g_custom_result_uri;
    g_custom_result_uri = nullptr;
  }
  SetScenario(0);
  CloseLogFile();
  TerminateCommon();
}

// Creates a global reference to the cursor retrieved by querying the FTL
// content provider. Caller is repsonsible for deleting the reference.
static jobject QueryContentProvider() {
  JNIEnv* env = g_app->GetJNIEnv();
  // contentResolver = activity.getContentResolver()
  jobject content_resolver = env->CallObjectMethod(
      g_app->activity(),
      util::activity::GetMethodId(util::activity::kGetContentResolver));
  if (util::CheckAndClearJniExceptions(env) || content_resolver == nullptr)
    return nullptr;
  jstring authority = env->NewStringUTF(kFirebaseTestLabAuthority);
  // authority = Uri.parse(authorityStr)
  jobject authority_uri = env->CallStaticObjectMethod(
      util::uri::GetClass(), util::uri::GetMethodId(util::uri::kParse),
      authority);
  util::CheckAndClearJniExceptions(env);
  // cursor = contentResolver.query(authority, null, null, null, null)
  jobject cursor_local = env->CallObjectMethod(
      content_resolver,
      util::content_resolver::GetMethodId(util::content_resolver::kQuery),
      authority_uri, nullptr, nullptr, nullptr, nullptr);
  jobject cursor = env->NewGlobalRef(cursor_local);
  env->DeleteLocalRef(cursor_local);
  env->DeleteLocalRef(content_resolver);
  env->DeleteLocalRef(authority);
  env->DeleteLocalRef(authority_uri);
  if (util::CheckAndClearJniExceptions(env) || cursor == nullptr)
    return nullptr;
  return cursor;
}

static int GetScenarioFromCursor(jobject cursor) {
  if (cursor == nullptr) return 0;
  JNIEnv* env = g_app->GetJNIEnv();
  // int scenarioCol = cursor.getColumnIndex(SCENARIO_COL);
  jstring scenario_col_name = env->NewStringUTF(kScenarioCol);
  int scenario_col = env->CallIntMethod(
      cursor, util::cursor::GetMethodId(util::cursor::kGetColumnIndex),
      scenario_col_name);
  env->DeleteLocalRef(scenario_col_name);
  if (util::CheckAndClearJniExceptions(env) || scenario_col == -1) return 0;

  // cursor.moveToFirst()
  env->CallBooleanMethod(cursor,
                         util::cursor::GetMethodId(util::cursor::kMoveToFirst));
  util::CheckAndClearJniExceptions(env);

  // cursor.getInt(scenarioCol)
  int scenario = env->CallIntMethod(
      cursor, util::cursor::GetMethodId(util::cursor::kGetInt), scenario_col);
  util::CheckAndClearJniExceptions(env);
  LogDebug("Retrieved scenario from the content provider: %d", scenario);
  return scenario;
}

static const char* GetResultsUriFromCursor(jobject cursor) {
  if (cursor == nullptr) return nullptr;
  JNIEnv* env = g_app->GetJNIEnv();
  // int customResultCol = cursor.getColumnIndex(CUSTOM_RESULT_COL);
  jstring custom_result_col_name = env->NewStringUTF(kCustomResultsCol);
  int custom_result_col = env->CallIntMethod(
      cursor, util::cursor::GetMethodId(util::cursor::kGetColumnIndex),
      custom_result_col_name);
  env->DeleteLocalRef(custom_result_col_name);
  if (util::CheckAndClearJniExceptions(env) || custom_result_col == -1)
    return nullptr;

  // cursor.moveToFirst()
  env->CallBooleanMethod(cursor,
                         util::cursor::GetMethodId(util::cursor::kMoveToFirst));
  util::CheckAndClearJniExceptions(env);

  // cursor.getString(customResultCol)
  jobject custom_result_str = env->CallObjectMethod(
      cursor, util::cursor::GetMethodId(util::cursor::kGetString),
      custom_result_col);
  util::CheckAndClearJniExceptions(env);
  const char* custom_result =
      util::JniStringToString(env, custom_result_str).c_str();
  LogDebug("Found the custom result uri string from the content provider: %s",
           custom_result);
  return custom_result;
}

// Attempt to initialize game loop scenario data from the Test Lab content
// provider and return whether successful
static bool InitFromContentProvider() {
  JNIEnv* env = g_app->GetJNIEnv();
  jobject cursor = QueryContentProvider();
  if (cursor == nullptr) {
    LogWarning(
        "Firebase Test Lab content provider does not exist or could not be "
        "queried.");
    return false;
  }

  int scenario = GetScenarioFromCursor(cursor);
  if (scenario == 0) return false;
  SetScenario(scenario);
  const char* custom_result = GetResultsUriFromCursor(cursor);
  env->DeleteGlobalRef(cursor);
  if (custom_result == nullptr) return false;
  g_custom_result_uri = new std::string(custom_result);

  return true;
}

// Helper method to initialize the game loop scenario number
static void InitFromIntent() {
  JNIEnv* env = g_app->GetJNIEnv();
  jobject activity = env->NewLocalRef(g_app->activity());

  // Intent intent = app.getIntent();
  jobject intent = env->CallObjectMethod(
      activity, util::activity::GetMethodId(util::activity::kGetIntent));
  if (util::CheckAndClearJniExceptions(env) || !intent) return;

  // scenario = intent.getIntExtra("scenario");
  jstring scenario_key = env->NewStringUTF("scenario");
  int scenario = env->CallIntMethod(
      intent, util::intent::GetMethodId(util::intent::kGetIntExtra),
      scenario_key, 0);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(intent);

  LogInfo("Received the scenario number %d", scenario);
  SetScenario(scenario);
  GetIntentUri();
}

// Helper method to retrieve the URI data from the intent
static void GetIntentUri() {
  JNIEnv* env = g_app->GetJNIEnv();
  jobject intent = env->CallObjectMethod(
      g_app->activity(),
      util::activity::GetMethodId(util::activity::kGetIntent));
  util::CheckAndClearJniExceptions(env);
  jobject uri = env->CallObjectMethod(
      intent, util::intent::GetMethodId(util::intent::kGetData));
  env->DeleteLocalRef(intent);
  util::CheckAndClearJniExceptions(env);
  if (uri == nullptr) {
    LogError(
        "Intent did not contain a valid file descriptor for the game "
        "loop custom results. If you manually set the scenario number, "
        "you must also provide a custom results directory or no results "
        "will be logged");
    return;
  }
  jobject uri_str =
      env->CallObjectMethod(uri, util::uri::GetMethodId(util::uri::kToString));
  env->DeleteLocalRef(uri);
  std::string custom_result_str = util::JniStringToString(env, uri_str);
  g_custom_result_uri = new std::string(custom_result_str);
}

// Helper method to initialize the custom results logging
FILE* RetrieveCustomResultsFile() {
  if (g_results_dir != nullptr) {
    return OpenCustomResultsFile(GetScenario());
  }
  if (g_custom_result_uri == nullptr) {
    LogError(
        "No URI of a custom results asset were found, no custom results will "
        "be logged.");
    return nullptr;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  // activity.getContentResolver()
  jobject content_resolver = env->CallObjectMethod(
      g_app->activity(),
      util::activity::GetMethodId(util::activity::kGetContentResolver));

  jstring uri_str = env->NewStringUTF(g_custom_result_uri->c_str());
  jobject uri = env->CallStaticObjectMethod(
      util::uri::GetClass(), util::uri::GetMethodId(util::uri::kParse),
      uri_str);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(uri_str);

  // contentResolver.openAssetFileDescriptor(Uri, String)
  jobject asset_file_descriptor = env->CallObjectMethod(
      content_resolver,
      util::content_resolver::GetMethodId(
          util::content_resolver::kOpenAssetFileDescriptor),
      uri, env->NewStringUTF("w"));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(uri);
  env->DeleteLocalRef(content_resolver);

  // assetFileDescriptor.getParcelFileDescriptor()
  jobject parcel_file_descriptor = env->CallObjectMethod(
      asset_file_descriptor,
      util::asset_file_descriptor::GetMethodId(
          util::asset_file_descriptor::kGetParcelFileDescriptor));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(asset_file_descriptor);

  // parcelFileDescriptor.detachFd()
  jint fd = env->CallIntMethod(parcel_file_descriptor,
                               util::parcel_file_descriptor::GetMethodId(
                                   util::parcel_file_descriptor::kDetachFd));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(parcel_file_descriptor);

  FILE* log_file = fdopen(fd, "w");
  if (env->ExceptionCheck() || log_file == nullptr) {
    LogError(
        "Firebase game loop custom results file could not be opened. Any "
        "logged results will not appear in the test's custom results.");
  }
  return log_file;
}

FILE* GetTempFile() {
  JNIEnv* env = g_app->GetJNIEnv();
  // File cache_dir_file = getCacheDir();
  jobject cache_dir_file = env->CallObjectMethod(
      g_app->activity(),
      util::activity::GetMethodId(util::activity::kGetCacheDir));
  if (util::CheckAndClearJniExceptions(env) || !cache_dir_file) {
    LogError("Could not obtain a temporary file");
    return nullptr;
  }

  // String cache_dir_path = cache_dir_file.getPath();
  jstring cache_dir_path = static_cast<jstring>(env->CallObjectMethod(
      cache_dir_file, util::file::GetMethodId(util::file::kGetPath)));
  std::string cache_dir_string = util::JniStringToString(env, cache_dir_path);
  env->DeleteLocalRef(cache_dir_file);
  if (util::CheckAndClearJniExceptions(env) || !cache_dir_path) {
    LogError("Could not obtain a temporary file");
    return nullptr;
  }

  std::string cache_file = cache_dir_string + "gameloopresultstemp.txt";
  return fopen(cache_file.c_str(), "w+");
}

void CallFinish() {
  JNIEnv* env = g_app->GetJNIEnv();
  jobject activity = env->NewLocalRef(g_app->activity());
  env->CallVoidMethod(activity,
                      util::activity::GetMethodId(util::activity::kFinish));
  env->DeleteLocalRef(activity);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace internal
}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase
