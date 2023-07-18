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

#ifndef FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_

#include "firebase/gma/ump.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

class ConsentInfoInternal {
 public:
  virtual ~ConsentInfoInternal();

  static ConsentInfoInternal* CreateInstance();

 protected:
  ConsentInfoInternal() {}
};

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_UMP_CONSENT_INFO_INTERNAL_H_
