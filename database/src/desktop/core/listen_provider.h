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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_LISTEN_PROVIDER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_LISTEN_PROVIDER_H_

#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/tag.h"
#include "database/src/desktop/view/view.h"

namespace firebase {
namespace database {
namespace internal {

// A class that is responsible for listening for changes on locations in the
// database, and relaying that data to the client.
class ListenProvider {
 public:
  virtual ~ListenProvider();

  // Begin listening on a location with a set of parameters given by the
  // QuerySpec. While listening, the server will send down updates which will be
  // parsed and passed along to the SyncTree to be cached locally.
  virtual void StartListening(const QuerySpec& query_spec, const Tag& tag,
                              const View* view) = 0;

  // Stop listening on a location given by the QuerySpec.
  virtual void StopListening(const QuerySpec& query_spec, const Tag& tag) = 0;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_LISTEN_PROVIDER_H_
