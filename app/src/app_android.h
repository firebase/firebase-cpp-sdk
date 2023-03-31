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

namespace firebase {
// Make sure the Java classes and methods are cached before triggering the
// the callback. Can be slow if this is called BEFORE any Firebase App is
// created. This is currently used by Unity SDK.
void CallAfterEnsureMethodsCached(JNIEnv* env, jobject activity,
                                  std::function<void()> callback);
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APP_ANDROID_H_
