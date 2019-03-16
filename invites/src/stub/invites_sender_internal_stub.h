// Copyright 2016 Google LLC
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

#ifndef FIREBASE_INVITES_CLIENT_CPP_SRC_STUB_INVITES_SENDER_INTERNAL_STUB_H_
#define FIREBASE_INVITES_CLIENT_CPP_SRC_STUB_INVITES_SENDER_INTERNAL_STUB_H_

#include "invites/src/common/invites_sender_internal.h"

namespace firebase {
class App;
namespace invites {
namespace internal {

// Stub version of InvitesSenderInternal, for use on desktop platforms. This
// version will simply not be able to send invitations, and will return an error
// if you try.
class InvitesSenderInternalStub : public InvitesSenderInternal {
 public:
  explicit InvitesSenderInternalStub(const ::firebase::App& app)
      : InvitesSenderInternal(app) {}
  virtual ~InvitesSenderInternalStub();  // NOLINT
  virtual bool PerformSendInvite();      // NOLINT
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_INVITES_CLIENT_CPP_SRC_STUB_INVITES_SENDER_INTERNAL_STUB_H_
