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
#ifndef FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_DESKTOP_INSTANCE_ID_INTERNAL_H_
#define FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_DESKTOP_INSTANCE_ID_INTERNAL_H_

#include "app/instance_id/instance_id_desktop_impl.h"
#include "app/src/include/firebase/app.h"
#include "app/src/safe_reference.h"
#include "instance_id/src/instance_id_internal_base.h"

namespace firebase {
namespace instance_id {
namespace internal {

class InstanceIdInternal : public InstanceIdInternalBase {
 public:
  explicit InstanceIdInternal(App* app);
  virtual ~InstanceIdInternal();

  InstanceIdDesktopImpl* impl() { return impl_; }

  // Safe reference to this.  Set in constructor and cleared in destructor
  // Should be safe to be copied in any thread because the SharedPtr never
  // changes, until safe_this_ is completely destroyed.
  typedef firebase::internal::SafeReference<InstanceIdInternal> InternalRef;
  typedef firebase::internal::SafeReferenceLock<InstanceIdInternal>
      InternalRefLock;
  InternalRef& safe_ref() { return safe_ref_; }

 public:
  InstanceIdDesktopImpl* impl_;

  InternalRef safe_ref_;
};

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase

#endif  // FIREBASE_INSTANCE_ID_CLIENT_CPP_SRC_DESKTOP_INSTANCE_ID_INTERNAL_H_
