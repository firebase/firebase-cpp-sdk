// Copyright 2023 Google LLC
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

#include "auth/src/include/firebase/auth_error.h"
#include "auth/src/common.h"  // For AuthData

namespace firebase {
namespace auth {

// Stub implementation for UseUserAccessGroupInternal on non-iOS platforms.
AuthError UseUserAccessGroupInternal(AuthData* auth_data,
                                     const char* group_id) {
  // This function is a no-op on non-iOS platforms.
  (void)auth_data;
  (void)group_id;
  return kAuthErrorNone;
}

// Other stub functions that are not implemented on desktop/non-mobile
// should also go here. For example, if there were other iOS or Android
// specific internal functions called from auth.cc

}  // namespace auth
}  // namespace firebase
