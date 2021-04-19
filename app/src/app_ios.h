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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_APP_IOS_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_APP_IOS_H_
#include "FIRApp.h"
#include "app/src/util_ios.h"

namespace firebase {

namespace internal {
OBJ_C_PTR_WRAPPER_NAMED(AppInternal, FIRApp);
}  // namespace internal

}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_APP_IOS_H_
