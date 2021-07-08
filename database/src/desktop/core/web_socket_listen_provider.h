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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CORE_WEB_SOCKET_LISTEN_PROVIDER_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CORE_WEB_SOCKET_LISTEN_PROVIDER_H_

#include "database/src/common/query_spec.h"
#include "database/src/desktop/connection/persistent_connection.h"
#include "database/src/desktop/core/listen_provider.h"
#include "database/src/desktop/core/repo.h"
#include "database/src/desktop/core/tag.h"

namespace firebase {
namespace database {
namespace internal {

class WebSocketListenProvider : public ListenProvider {
 public:
  WebSocketListenProvider(Repo* repo,
                          connection::PersistentConnection* connection,
                          Logger* logger)
      : repo_(repo),
        sync_tree_(nullptr),
        connection_(connection),
        logger_(logger) {}

  ~WebSocketListenProvider() override {}

  void set_sync_tree(SyncTree* sync_tree) { sync_tree_ = sync_tree; }

  void StartListening(const QuerySpec& query_spec, const Tag& tag,
                      const View* view) override;

  void StopListening(const QuerySpec& query_spec, const Tag& tag) override;

 private:
  Repo* repo_;
  SyncTree* sync_tree_;
  connection::PersistentConnection* connection_;
  Logger* logger_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CORE_WEB_SOCKET_LISTEN_PROVIDER_H_
