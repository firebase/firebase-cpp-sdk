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

#include <string>
#include <utility>

#include "Firestore/core/src/util/status.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/service_interfaces.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"

namespace firebase {
namespace firestore {

using credentials::CredentialChangeListener;
using credentials::TokenListener;

CppAppCheckCredentialsProvider::CppAppCheckCredentialsProvider(
    ::firebase::internal::AppState& app_state)
    : app_state_(app_state) {}

CppAppCheckCredentialsProvider::~CppAppCheckCredentialsProvider() {
  RemoveAppCheckStateListener();
}

void CppAppCheckCredentialsProvider::SetCredentialChangeListener(
    CredentialChangeListener<std::string> listener) {
  if (!listener) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      SIMPLE_HARD_ASSERT(change_listener_,
                         "Change listener removed without being set!");
      change_listener_ = {};
    }
    RemoveAppCheckStateListener();
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    SIMPLE_HARD_ASSERT(!change_listener_, "Set change listener twice!");
    change_listener_ = std::move(listener);
  }

  AddAppCheckStateListener();
}

void CppAppCheckCredentialsProvider::GetToken(
    TokenListener<std::string> listener) {
  if (auto* app_check = app_state_.app_check_token_provider()) {
    Future<std::string> app_check_future = app_check->GetTokenAsync();
    if (app_check_future.status() != kFutureStatusInvalid) {
      app_check_future.OnCompletion(
          [listener =
               std::move(listener)](const Future<std::string>& future_token) {
            if (future_token.result()) {
              listener(*future_token.result());
            } else {
              listener(std::string());
            }
          });
      return;
    }
  }
  // App Check is unavailable, so assume there is no App Check token to use.
  listener(std::string());
}

void CppAppCheckCredentialsProvider::AddAppCheckStateListener() {
  if (auto* app_check = app_state_.app_check_token_provider()) {
    app_check->AddTokenListener(OnAppCheckStateChanged, this);
  }
}

void CppAppCheckCredentialsProvider::RemoveAppCheckStateListener() {
  if (auto* app_check = app_state_.app_check_token_provider()) {
    app_check->RemoveTokenListener(OnAppCheckStateChanged, this);
  }
}

void CppAppCheckCredentialsProvider::OnAppCheckStateChanged(
    const std::string& token, void* context) {
  auto provider = static_cast<CppAppCheckCredentialsProvider*>(context);
  CredentialChangeListener<std::string> listener;
  {
    std::lock_guard<std::mutex> lock(provider->mutex_);
    listener = provider->change_listener_;
  }
  if (listener) {
    listener(token);
  }
}

}  // namespace firestore
}  // namespace firebase
