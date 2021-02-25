// Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIS_CLIENT_CPP_SRC_INSTALLATIONS_INTERNAL_H_
#define FIREBASE_FIS_CLIENT_CPP_SRC_INSTALLATIONS_INTERNAL_H_

// InstallationsInternal is defined in these 2 files, one implementation for
// each OS.
#if FIREBASE_PLATFORM_ANDROID || FIREBASE_ANDROID_FOR_DESKTOP
#include "installations/src/android/installations_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "installations/src/ios/installations_ios.h"
#else
#include "installations/src/stub/installations_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

#endif  // FIREBASE_FIS_CLIENT_CPP_SRC_INSTALLATIONS_INTERNAL_H_
