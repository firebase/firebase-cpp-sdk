/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_SRC_APP_ANDROID_H_
#define FIREBASE_APP_SRC_APP_ANDROID_H_

#include <jni.h>

#include <functional>

#include "app/src/jni/env.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "firebase/variant.h"

namespace firebase {
// Make sure the Java classes and methods are cached before triggering the
// the callback. Can be slow if this is called BEFORE any Firebase App is
// created. This is currently used by Unity SDK.
void CallAfterEnsureMethodsCached(JNIEnv* env, jobject activity,
                                  std::function<void()> callback);

namespace util {

JNIEnv* GetThreadsafeJNIEnv(JavaVM* vm);
JNIEnv* GetJNIEnvFromApp();
void RunOnMainThread(JNIEnv* env, jobject activity, void (*callback)(void*), void* data);
void CancelRunOnMainThread(void* data);

firebase::Variant JavaObjectToVariant(JNIEnv* env, jobject obj);
jobject VariantToJavaObject(JNIEnv* env, const firebase::Variant& variant);

enum FutureResult {
  kFutureResultSuccess,
  kFutureResultFailure,
  kFutureResultCancelled,
};

typedef void TaskCallbackFn(JNIEnv* env, jobject result, FutureResult result_code, const char* status_message, void* callback_data);
void RegisterCallbackOnTask(JNIEnv* env, jobject task, TaskCallbackFn* callback_fn, void* callback_data, const char* api_identifier);

void CancelCallbacks(JNIEnv* env, const char* class_method_identifier);

bool LogException(JNIEnv* env, int log_level, const char* message, ...);

int64_t JLongToInt64(jlong j_long);
jobject CharsToURL(JNIEnv* env, const char* url_string);
jobject ContinueBuilder(JNIEnv* env, jobject old_builder, jobject new_builder);

}  // namespace util
}  // namespace firebase


#endif  // FIREBASE_APP_SRC_APP_ANDROID_H_
