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
#include "database/src/desktop/core/tag.h"
#include "database/src/desktop/view/view.h"

namespace firebase {
namespace database {
namespace internal {

using connection::PersistentConnection;
using connection::ResponsePtr;

class WebSocketListenResponse : public connection::Response {
 public:
  WebSocketListenResponse(const Response::ResponseCallback& callback,
                          const Repo::ThisRef& repo_ref, SyncTree* sync_tree,
                          const QuerySpec& query_spec, const Tag& tag,
                          const View* view, Logger* logger)
      : connection::Response(callback),
        repo_ref_(repo_ref),
        sync_tree_(sync_tree),
        query_spec_(query_spec),
        tag_(tag),
        view_(view),
        logger_(logger) {}

  Repo::ThisRef& repo_ref() { return repo_ref_; }
  SyncTree* sync_tree() { return sync_tree_; }
  const QuerySpec& query_spec() const { return query_spec_; }
  const Tag& tag() const { return tag_; }
  const View* view() const { return view_; }
  Logger* logger() { return logger_; }

 private:
  Repo::ThisRef repo_ref_;
  SyncTree* sync_tree_;
  QuerySpec query_spec_;
  Tag tag_;
  const View* view_;
  Logger* logger_;
};

void WebSocketListenProvider::StartListening(const QuerySpec& query_spec,
                                             const Tag& tag, const View* view) {
  connection_->Listen(
      query_spec, tag,
      MakeShared<WebSocketListenResponse>(
          [](const SharedPtr<connection::Response>& connection_response) {
            WebSocketListenResponse* response =
                static_cast<WebSocketListenResponse*>(
                    connection_response.get());
            Logger* logger = response->logger();

            Repo::ThisRefLock lock(&response->repo_ref());
            Repo* repo = lock.GetReference();
            if (repo == nullptr) {
              // Repo was deleted, do not proceed.
              return;
            }

            std::vector<Event> events;
            if (!response->HasError()) {
              const QuerySpec& query_spec = response->view()->query_spec();
              const Tag& tag = response->tag();
              if (tag.has_value()) {
                events = response->sync_tree()->ApplyTaggedListenComplete(tag);
              } else {
                events =
                    response->sync_tree()->ApplyListenComplete(query_spec.path);
              }
            } else {
              logger->LogWarning("Listen at %s failed: %s",
                                 response->query_spec().path.c_str(),
                                 response->GetErrorMessage().c_str());

              // If a listen failed, kill all of the listeners here, not just
              // the one that triggered the error. Note that this may need to be
              // scoped to just this listener if we change permissions on
              // filtered children
              events = response->sync_tree()->RemoveAllEventRegistrations(
                  response->query_spec(), response->GetErrorCode());
            }
            repo->PostEvents(events);
          },
          repo_->this_ref(), sync_tree_, query_spec, tag, view, logger_));
}

void WebSocketListenProvider::StopListening(const QuerySpec& query_spec,
                                            const Tag& tag) {
  connection_->Unlisten(query_spec);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
