/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_ADMOB_ANDROID_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_ADMOB_ANDROID_H_

#include <jni.h>

namespace firebase {
namespace admob {

// Change codes used when receiving state change callbacks from the Java
// BannerViewHelper and NativeExpressAdViewHelper objects.
enum AdViewChangeCode {
  // The callback indicates the presentation state has changed.
  kChangePresentationState = 0,
  // The callback indicates the bounding box has changed.
  kChangeBoundingBox,
  kChangeCount
};

// Needed when AdMob is initialized without Firebase.
JNIEnv* GetJNI();

// Retrieves the activity used to initalize AdMob.
jobject GetActivity();

// Register the native callbacks needed by the Futures.
bool RegisterNatives();

// Release classes registered by this module.
void ReleaseClasses(JNIEnv* env);

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_ADMOB_ANDROID_H_
