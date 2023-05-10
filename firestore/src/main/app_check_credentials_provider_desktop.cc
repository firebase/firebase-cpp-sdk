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
#include "app/src/function_registry.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"

namespace firebase {
namespace firestore {

using credentials::CredentialChangeListener;
using credentials::TokenListener;

CppAppCheckCredentialsProvider::CppAppCheckCredentialsProvider(App& app)
    : app_(app) {}

CppAppCheckCredentialsProvider::~CppAppCheckCredentialsProvider() {
  RemoveAppCheckStateListener();
}

void CppAppCheckCredentialsProvider::SetCredentialChangeListener(
    CredentialChangeListener<std::string> listener) {
  if (!listener) {
    SIMPLE_HARD_ASSERT(change_listener_,
                       "Change listener removed without being set!");
    change_listener_ = {};
    RemoveAppCheckStateListener();
    return;
  }

  SIMPLE_HARD_ASSERT(!change_listener_, "Set change listener twice!");
  change_listener_ = std::move(listener);

  AddAppCheckStateListener();
}

void CppAppCheckCredentialsProvider::GetToken(
    TokenListener<std::string> listener) {
  // Get token, tell the listener
  Future<std::string> app_check_future;
  bool succeeded = app_.function_registry()->CallFunction(
      ::firebase::internal::FnAppCheckGetTokenAsync, &app_, nullptr,
      &app_check_future);
  if (succeeded && app_check_future.status() != kFutureStatusInvalid) {
    app_check_future.OnCompletion(
        [listener](const Future<std::string>& future_token) {
          if (future_token.result()) {
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
  auto callback = reinterpret_cast<void*>(OnAppCheckStateChanged);
  app_.function_registry()->CallFunction(
      ::firebase::internal::FnAppCheckAddListener, &app_, callback, this);
}

void CppAppCheckCredentialsProvider::RemoveAppCheckStateListener() {
  auto callback = reinterpret_cast<void*>(OnAppCheckStateChanged);
  app_.function_registry()->CallFunction(
      ::firebase::internal::FnAppCheckRemoveListener, &app_, callback, this);
}

void CppAppCheckCredentialsProvider::OnAppCheckStateChanged(
    const std::string& token, void* context) {
  auto provider = static_cast<CppAppCheckCredentialsProvider*>(context);

  if (provider->change_listener_) {
    provider->change_listener_(token);
  }
}

}  // namespace firestore
}  // namespace firebase
