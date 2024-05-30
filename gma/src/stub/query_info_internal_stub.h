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

#ifndef FIREBASE_GMA_SRC_STUB_QUERY_INFO_INTERNAL_STUB_H_
#define FIREBASE_GMA_SRC_STUB_QUERY_INFO_INTERNAL_STUB_H_

#include "gma/src/common/query_info_internal.h"

namespace firebase {
namespace gma {
namespace internal {

/// Stub version of QueryInfoInternal, for use on desktop platforms. GMA
/// is forbidden on desktop, so this version creates and immediately completes
/// the Future for each method.
class QueryInfoInternalStub : public QueryInfoInternal {
 public:
  explicit QueryInfoInternalStub(QueryInfo* base) : QueryInfoInternal(base) {}

  ~QueryInfoInternalStub() override {}

  Future<void> Initialize(AdParent parent) override {
    return CreateAndCompleteFutureStub(kQueryInfoFnInitialize);
  }

  Future<QueryInfoResult> CreateQueryInfo(AdFormat format,
                                          const AdRequest& request) override {
    return CreateAndCompleteQueryInfoFutureStub(kQueryInfoFnCreateQueryInfo);
  }

  Future<QueryInfoResult> CreateQueryInfoWithAdUnit(
      AdFormat format, const AdRequest& request,
      const char* ad_unit_id) override {
    return CreateAndCompleteQueryInfoFutureStub(
        kQueryInfoFnCreateQueryInfoWithAdUnit);
  }

  bool is_initialized() const override { return true; }

 private:
  Future<void> CreateAndCompleteFutureStub(QueryInfoFn fn) {
    CreateAndCompleteFuture(fn, kAdErrorCodeNone, nullptr, &future_data_);
    return GetInitializeLastResult();
  }

  Future<QueryInfoResult> CreateAndCompleteQueryInfoFutureStub(QueryInfoFn fn) {
    return CreateAndCompleteFutureWithQueryInfoResult(
        fn, kAdErrorCodeNone, nullptr, &future_data_, QueryInfoResult());
  }
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_STUB_QUERY_INFO_INTERNAL_STUB_H_
