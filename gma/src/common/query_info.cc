/*
 * Copyright 2024 Google LLC
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

#include "gma/src/include/firebase/gma/internal/query_info.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/common/query_info_internal.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

QueryInfo::QueryInfo() {
  FIREBASE_ASSERT(gma::IsInitialized());
  internal_ = internal::QueryInfoInternal::CreateInstance(this);

  GetOrCreateCleanupNotifier()->RegisterObject(this, [](void* object) {
    LogWarning("QueryInfo must be deleted before gma::Terminate.");
    QueryInfo* query_info = reinterpret_cast<QueryInfo*>(object);
    delete query_info->internal_;
    query_info->internal_ = nullptr;
  });
}

QueryInfo::~QueryInfo() {
  FIREBASE_ASSERT(internal_);

  GetOrCreateCleanupNotifier()->UnregisterObject(this);
  delete internal_;
}

// Initialize must be called before any other methods in the namespace. This
// method asserts that Initialize() has been invoked and allowed to complete.
static bool CheckIsInitialized(internal::QueryInfoInternal* internal) {
  FIREBASE_ASSERT(internal);
  return internal->is_initialized();
}

Future<void> QueryInfo::Initialize(AdParent parent) {
  return internal_->Initialize(parent);
}

Future<void> QueryInfo::InitializeLastResult() const {
  return internal_->GetInitializeLastResult();
}

Future<QueryInfoResult> QueryInfo::CreateQueryInfo(AdFormat format,
                                                   const AdRequest& request) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithQueryInfoResult(
        firebase::gma::internal::kQueryInfoFnCreateQueryInfo,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, QueryInfoResult());
  }

  return internal_->CreateQueryInfo(format, request);
}

Future<QueryInfoResult> QueryInfo::CreateQueryInfoLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithQueryInfoResult(
        firebase::gma::internal::kQueryInfoFnCreateQueryInfo,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, QueryInfoResult());
  }
  return internal_->CreateQueryInfoLastResult(
      internal::kQueryInfoFnCreateQueryInfo);
}

Future<QueryInfoResult> QueryInfo::CreateQueryInfoWithAdUnit(
    AdFormat format, const AdRequest& request, const char* ad_unit_id) {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithQueryInfoResult(
        firebase::gma::internal::kQueryInfoFnCreateQueryInfoWithAdUnit,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, QueryInfoResult());
  }

  return internal_->CreateQueryInfoWithAdUnit(format, request, ad_unit_id);
}

Future<QueryInfoResult> QueryInfo::CreateQueryInfoWithAdUnitLastResult() const {
  if (!CheckIsInitialized(internal_)) {
    return CreateAndCompleteFutureWithQueryInfoResult(
        firebase::gma::internal::kQueryInfoFnCreateQueryInfoWithAdUnit,
        kAdErrorCodeUninitialized, kAdUninitializedErrorMessage,
        &internal_->future_data_, QueryInfoResult());
  }
  return internal_->CreateQueryInfoLastResult(
      internal::kQueryInfoFnCreateQueryInfoWithAdUnit);
}

}  // namespace gma
}  // namespace firebase
