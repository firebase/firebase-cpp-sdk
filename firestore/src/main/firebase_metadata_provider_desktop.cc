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
  if (heartbeat_controller_) {
    std::string payload =
        heartbeat_controller_->GetAndResetTodaysStoredHeartbeats();
    // The payload is either an empty string or a string of user agents to log.
    if (!payload.empty()) {
      context.AddMetadata(kXFirebaseClientLogTypeHeader, kHeartbeatCodeGlobal);
      context.AddMetadata(kXFirebaseClientHeader, payload);
    }
  }
  if (!gmp_app_id_.empty()) {
    context.AddMetadata(kXFirebaseGmpIdHeader, gmp_app_id_);
  }
}

}  // namespace firestore
}  // namespace firebase
