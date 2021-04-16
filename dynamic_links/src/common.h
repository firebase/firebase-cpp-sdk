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

#ifndef FIREBASE_DYNAMIC_LINKS_CLIENT_CPP_SRC_COMMON_H_
#define FIREBASE_DYNAMIC_LINKS_CLIENT_CPP_SRC_COMMON_H_

#include "app/src/include/firebase/app.h"
#include "app/src/reference_counted_future_impl.h"
#include "dynamic_links/src/include/firebase/dynamic_links.h"

namespace firebase {
namespace dynamic_links {

enum DynamicLinksFn { kDynamicLinksFnGetShortLink, kDynamicLinksFnCount };

// Data structure which holds the Future API implementation with the
// future required by this API.
class FutureData {
 public:
  FutureData() : api_(kDynamicLinksFnCount) {}
  ~FutureData() {}

  ReferenceCountedFutureImpl* api() { return &api_; }

  // Create the FutureData singleton.
  static FutureData* Create();
  // Destroy the FutureData singleton.
  static void Destroy();
  // Get the FutureData singleton.
  static FutureData* Get();

 private:
  ReferenceCountedFutureImpl api_;

  static FutureData* s_future_data_;
};

// Create the dynamic links receiver.
bool CreateReceiver(const App& app);
// Destroy the dynamic links receiver.
void DestroyReceiver();

}  // namespace dynamic_links
}  // namespace firebase

#endif  // FIREBASE_DYNAMIC_LINKS_CLIENT_CPP_SRC_COMMON_H_
