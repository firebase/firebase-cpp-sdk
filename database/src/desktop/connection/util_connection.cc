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

#include "database/src/desktop/connection/util_connection.h"

#include "database/src/desktop/connection/web_socket_client_impl.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

UniquePtr<WebSocketClientInterface> CreateWebSocketClient(
    const HostInfo& info, WebSocketClientEventHandler* delegate,
    const char* opt_last_session_id, Logger* logger,
    scheduler::Scheduler* scheduler) {
  // Currently we use uWebSockets implementation.
  std::string uri = info.GetConnectionUrl(opt_last_session_id);
  return MakeUnique<WebSocketClientImpl>(uri, info.user_agent(), logger,
                                         scheduler, delegate);
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
