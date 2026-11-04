/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/main/app_check_credentials_provider_desktop.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <atomic>
#include <unordered_map>
#include <utility>

#include "Firestore/core/src/util/status.h"
#include "app/src/function_registry.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"

namespace firebase {
namespace firestore {

using credentials::CredentialChangeListener;
using credentials::TokenListener;

namespace {

std::recursive_mutex& ListenerMutex() {
  static std::recursive_mutex mutex;
  return mutex;
}

std::unordered_map<uint64_t, CppAppCheckCredentialsProvider*>& ValidProviders() {
  static std::unordered_map<uint64_t, CppAppCheckCredentialsProvider*> providers;
  return providers;
}

uint64_t GetNextProviderId() {
  static std::atomic<uint64_t> next_id{1};
  return next_id.fetch_add(1);
}

}  // namespace

CppAppCheckCredentialsProvider::CppAppCheckCredentialsProvider(App& app)
    : app_(app), provider_id_(GetNextProviderId()) {
  std::scoped_lock lock(ListenerMutex());
  ValidProviders()[provider_id_] = this;
}

CppAppCheckCredentialsProvider::~CppAppCheckCredentialsProvider() {
  RemoveAppCheckStateListener();
  std::scoped_lock lock(ListenerMutex());
  ValidProviders().erase(provider_id_);
}

void CppAppCheckCredentialsProvider::SetCredentialChangeListener(
    CredentialChangeListener<std::string> listener) {
  bool has_listener = static_cast<bool>(listener);
  {
    std::scoped_lock lock(ListenerMutex());
    if (!has_listener) {
      SIMPLE_HARD_ASSERT(change_listener_,
                         "Change listener removed without being set!");
      change_listener_ = {};
    } else {
      SIMPLE_HARD_ASSERT(!change_listener_, "Set change listener twice!");
      change_listener_ = std::move(listener);
    }
  }
  if (!has_listener) {
    RemoveAppCheckStateListener();
  } else {
    AddAppCheckStateListener();
  }
}

void CppAppCheckCredentialsProvider::GetToken(
    TokenListener<std::string> listener) {
    bool force_refresh = force_refresh_.exchange(false);
  // Get token, tell the listener
  Future<std::string> app_check_future;
  bool succeeded = app_.function_registry()->CallFunction(
      ::firebase::internal::FnAppCheckGetTokenAsync, &app_,
      &force_refresh, &app_check_future);
  if (succeeded && app_check_future.status() != kFutureStatusInvalid) {
    app_check_future.OnCompletion(
        [listener = std::move(listener)](
            const Future<std::string>& future_token) {
          if (future_token.error() != 0) {
            listener(std::string());
          } else if (future_token.result()) {
            listener(*future_token.result());
          } else {
            listener(std::string());
          }
        });
  } else {
    // Getting the Future failed, so assume there is no App Check token to use.
    listener(std::string());
  }
}

void CppAppCheckCredentialsProvider::AddAppCheckStateListener() {
  auto callback = reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&OnAppCheckStateChanged));
  app_.function_registry()->CallFunction(
      ::firebase::internal::FnAppCheckAddListener, &app_, callback, reinterpret_cast<void*>(static_cast<uintptr_t>(provider_id_)));
}

void CppAppCheckCredentialsProvider::RemoveAppCheckStateListener() {
  auto callback = reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(&OnAppCheckStateChanged));
  app_.function_registry()->CallFunction(
      ::firebase::internal::FnAppCheckRemoveListener, &app_, callback, reinterpret_cast<void*>(static_cast<uintptr_t>(provider_id_)));
}

void CppAppCheckCredentialsProvider::OnAppCheckStateChanged(
    const std::string& token, void* context) {
  auto provider_id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(context));
  
  CredentialChangeListener<std::string> listener;
  {
    std::scoped_lock lock(ListenerMutex());
    auto it = ValidProviders().find(provider_id);
    if (it == ValidProviders().end()) return;
    listener = it->second->change_listener_;
  }

  if (listener) {
    listener(token);
  }
}

}  // namespace firestore
}  // namespace firebase
