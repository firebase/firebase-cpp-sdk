// Copyright 2019 Google LLC
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

#include "database/src/desktop/core/web_socket_listen_provider.h"

#include "database/src/common/query_spec.h"
#include "database/src/desktop/connection/persistent_connection.h"
#include "database/src/desktop/core/listen_provider.h"

namespace firebase {
namespace database {
namespace internal {

using connection::PersistentConnection;
using connection::ResponsePtr;

void WebSocketListenProvider::StartListening(const QuerySpec& query_spec) {
  connection_->Listen(query_spec, PersistentConnection::Tag(), ResponsePtr());
}

void WebSocketListenProvider::StopListening(const QuerySpec& query_spec) {
  connection_->Unlisten(query_spec);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
