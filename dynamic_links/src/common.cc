// Copyright 2017 Google LLC
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

#include "dynamic_links/src/common.h"

#include <assert.h>

#include "app/src/cleanup_notifier.h"
#include "app/src/invites/cached_receiver.h"
#include "app/src/invites/invites_receiver_internal.h"
#include "app/src/util.h"
#include "dynamic_links/src/include/firebase/dynamic_links.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(dynamic_links,
                                {
                                  if (app == ::firebase::App::GetInstance()) {
                                    return firebase::dynamic_links::Initialize(
                                        *app, nullptr);
                                  }
                                  return kInitResultSuccess;
                                },
                                {
                                  if (app == ::firebase::App::GetInstance()) {
                                    firebase::dynamic_links::Terminate();
                                  }
                                });

namespace firebase {
namespace dynamic_links {

namespace internal {
const char kDynamicLinksModuleName[] = "dynamic_links";
}  // namespace internal

// Notifies a listener of a cached invite.
class CachedListenerNotifier : public invites::internal::ReceiverInterface {
 public:
  CachedListenerNotifier() : listener_(nullptr) {}
  virtual ~CachedListenerNotifier() { SetListener(nullptr); }

  // Set the listener which should be notified of any cached or receiver
  // links.
  Listener* SetListener(Listener* listener) {
    MutexLock lock(lock_);
    Listener* previous_listener = listener_;
    listener_ = listener;
    receiver_.SetReceiver(listener ? this : nullptr);
    return previous_listener;
  }

 private:
  // Callback called when an invite is received. If an error occurred,
  // result_code should be non-zero. Otherwise, either invitation_id should be
  // set, or deep_link_url should be set, or both.
  void ReceivedInviteCallback(
      const std::string& invitation_id, const std::string& deep_link_url,
      invites::internal::InternalLinkMatchStrength match_strength,
      int result_code, const std::string& error_message) override {
    MutexLock lock(lock_);
    if (listener_) {
      if (!deep_link_url.empty()) {
        DynamicLink link;
        link.url = deep_link_url;
        link.match_strength = static_cast<LinkMatchStrength>(match_strength);
        listener_->OnDynamicLinkReceived(&link);
      }
    } else {
      receiver_.ReceivedInviteCallback(invitation_id, deep_link_url,
                                       match_strength, result_code,
                                       error_message);
    }
  }

 private:
  // Protects access to members of this class.
  Mutex lock_;
  // End user's listener which is notified of invites.
  Listener* listener_;
  // Caches received links.
  invites::internal::CachedReceiver receiver_;
};

static invites::internal::InvitesReceiverInternal* g_receiver = nullptr;
static CachedListenerNotifier* g_cached_receiver = nullptr;

FutureData* FutureData::s_future_data_ = nullptr;

// Create FutureData singleton.
FutureData* FutureData::Create() {
  s_future_data_ = new FutureData();
  return s_future_data_;
}

// Destroy the FutureData singleton.
void FutureData::Destroy() {
  delete s_future_data_;
  s_future_data_ = nullptr;
}

// Get the Future data singleton.
FutureData* FutureData::Get() { return s_future_data_; }

bool CreateReceiver(const App& app) {
  assert(!g_cached_receiver && !g_receiver);
  g_cached_receiver = new CachedListenerNotifier();
  g_receiver = invites::internal::InvitesReceiverInternal::CreateInstance(
      app, g_cached_receiver);
  if (!g_receiver) {
    delete g_cached_receiver;
    g_cached_receiver = nullptr;
    return false;
  }
  if (!AppCallback::GetEnabledByName(internal::kDynamicLinksModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(const_cast<App*>(g_receiver->app()));
    assert(cleanup_notifier);
    cleanup_notifier->RegisterObject(
        const_cast<char*>(internal::kDynamicLinksModuleName), [](void*) {
          LogError(
              "dynamic_links::Terminate() should be called before the "
              "default app is destroyed.");
          if (g_receiver) firebase::dynamic_links::Terminate();
        });
  }
  return true;
}

void DestroyReceiver() {
  assert(g_cached_receiver && g_receiver);
  if (!AppCallback::GetEnabledByName(internal::kDynamicLinksModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(const_cast<App*>(g_receiver->app()));
    assert(cleanup_notifier);
    cleanup_notifier->UnregisterObject(
        const_cast<char*>(internal::kDynamicLinksModuleName));
  }
  SetListener(nullptr);
  invites::internal::InvitesReceiverInternal::DestroyInstance(
      g_receiver, g_cached_receiver);
  g_receiver = nullptr;
  delete g_cached_receiver;
  g_cached_receiver = nullptr;
}

Listener* SetListener(Listener* listener) {
  if (g_cached_receiver == nullptr) return nullptr;

  if (listener) {
    Fetch();
  }

  return g_cached_receiver->SetListener(listener);
}

void Fetch() {
  if (g_receiver) g_receiver->Fetch();
}

}  // namespace dynamic_links
}  // namespace firebase
