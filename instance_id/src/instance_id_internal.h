// Copyright 2017 Google LLC
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

#ifndef FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_INSTANCE_ID_INTERNAL_H_
#define FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_INSTANCE_ID_INTERNAL_H_

#if defined(__APPLE__)
#include "TargetConditionals.h"
#endif  // defined(__APPLE__)

#if defined(__ANDROID__)
#include "instance_id/src/android/instance_id_internal.h"
#elif TARGET_OS_IPHONE
#include "instance_id/src/ios/instance_id_internal.h"
#else
#include "instance_id/src/stub/instance_id_internal.h"
#endif

#endif  // FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_INSTANCE_ID_INTERNAL_H_
