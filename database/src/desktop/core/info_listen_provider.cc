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

#include "database/src/desktop/core/info_listen_provider.h"

#include "database/src/common/query_spec.h"
#include "database/src/desktop/connection/persistent_connection.h"
#include "database/src/desktop/core/listen_provider.h"
#include "database/src/desktop/core/tag.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/view.h"

namespace firebase {
namespace database {
namespace internal {

void InfoListenProvider::StartListening(const QuerySpec& query_spec,
                                        const Tag& tag, const View* view) {
  repo_->scheduler().Schedule([this, query_spec]() {
    const Variant& value = VariantGetChild(info_data_, query_spec.path);
    if (!VariantIsEmpty(value)) {
      repo_->PostEvents(
          sync_tree_->ApplyServerOverwrite(query_spec.path, value));
    }
  });
}

void InfoListenProvider::StopListening(const QuerySpec& query_spec,
                                       const Tag& tag) {}

}  // namespace internal
}  // namespace database
}  // namespace firebase
