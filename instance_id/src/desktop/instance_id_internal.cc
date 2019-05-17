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

#include "instance_id/src/desktop/instance_id_internal.h"

namespace firebase {
namespace instance_id {
namespace internal {

InstanceIdInternal::InstanceIdInternal(App* app)
    : InstanceIdInternalBase(), safe_ref_(this) {
  impl_ = InstanceIdDesktopImpl::GetInstance(app);
}

InstanceIdInternal::~InstanceIdInternal() {
  safe_ref_.ClearReference();
  // App will make sure impl_ gets deleted.
}

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase
