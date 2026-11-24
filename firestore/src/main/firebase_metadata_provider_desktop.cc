/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/main/firebase_metadata_provider_desktop.h"

#include <utility>

#include "app/src/include/firebase/app.h"

namespace firebase {
namespace firestore {

const char* kHeartbeatCodeGlobal = "2";

FirebaseMetadataProviderCpp::FirebaseMetadataProviderCpp(const App& app)
    : heartbeat_controller_(std::move(app.GetHeartbeatController())),
      gmp_app_id_(app.options().app_id()) {}

void FirebaseMetadataProviderCpp::UpdateMetadata(grpc::ClientContext& context) {
  // Flag at most one request per day as today's heartbeat. The stored payload
  // is only used to decide whether today's heartbeat is still unsent.
  if (heartbeat_controller_ &&
      !heartbeat_controller_->GetAndResetTodaysStoredHeartbeats().empty()) {
    context.AddMetadata(kXFirebaseClientLogTypeHeader, kHeartbeatCodeGlobal);
  }
  // Always send the current user agent, as the Android and iOS SDKs do. The
  // stored heartbeat is logged when the App is created, before Firestore
  // registers its library, so it cannot be used to attribute Firestore usage.
  const char* user_agent = App::GetUserAgent();
  if (user_agent != nullptr && user_agent[0] != '\0') {
    context.AddMetadata(kXFirebaseClientHeader, user_agent);
  }
  if (!gmp_app_id_.empty()) {
    context.AddMetadata(kXFirebaseGmpIdHeader, gmp_app_id_);
  }
}

}  // namespace firestore
}  // namespace firebase
