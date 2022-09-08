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

#include "app/src/heartbeat_info_desktop.h"
#include "app/src/include/firebase/app.h"

namespace firebase {
namespace firestore {

FirebaseMetadataProviderCpp::FirebaseMetadataProviderCpp(App* app)
    : app_(app), gmp_app_id_(app->options().app_id()) {}

void FirebaseMetadataProviderCpp::UpdateMetadata(grpc::ClientContext& context) {
  HeartbeatInfo::Code heartbeat = HeartbeatInfo::GetHeartbeatCode(app_);

  // TODO(varconst): don't send any headers if the heartbeat is "none". This
  // should only be changed once it's possible to notify the heartbeat that the
  // previous attempt to send it has failed.
  if (heartbeat != HeartbeatInfo::Code::None) {
    context.AddMetadata(kXFirebaseClientLogTypeHeader,
                        std::to_string(static_cast<int>(heartbeat)));
  }

  context.AddMetadata(kXFirebaseClientHeader, App::GetUserAgent());

  if (!gmp_app_id_.empty()) {
    context.AddMetadata(kXFirebaseGmpIdHeader, gmp_app_id_);
  }
}

}  // namespace firestore
}  // namespace firebase
