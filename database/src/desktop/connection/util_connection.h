// Copyright 2018 Google LLC
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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_UTIL_CONNECTION_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_UTIL_CONNECTION_H_
#include "app/memory/unique_ptr.h"
#include "app/src/logger.h"
#include "app/src/scheduler.h"
#include "database/src/desktop/connection/host_info.h"
#include "database/src/desktop/connection/web_socket_client_interface.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

// Helper function to create a websocket client regardless its implementation or
// platform
UniquePtr<WebSocketClientInterface> CreateWebSocketClient(
    const HostInfo& info, WebSocketClientEventHandler* delegate,
    const char* opt_last_session_id, Logger* logger,
    scheduler::Scheduler* scheduler);

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CONNECTION_UTIL_CONNECTION_H_
