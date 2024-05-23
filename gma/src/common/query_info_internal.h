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

#ifndef FIREBASE_GMA_SRC_COMMON_QUERY_INFO_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_QUERY_INFO_INTERNAL_H_

#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/internal/query_info.h"

namespace firebase {
namespace gma {
namespace internal {

// Constants representing each NativeAd function that returns a Future.
enum QueryInfoFn {
  kQueryInfoFnInitialize,
  kQueryInfoFnCreateQueryInfo,
  kQueryInfoFnCreateQueryInfoWithAdUnit,
  kQueryInfoFnCount
};

class QueryInfoInternal {
 public:
  // Create an instance of whichever subclass of QueryInfoInternal is
  // appropriate for the current platform.
  static QueryInfoInternal* CreateInstance(QueryInfo* base);

  // Virtual destructor is required.
  virtual ~QueryInfoInternal() = default;

  // Initializes this object and any platform-specific helpers that it uses.
  virtual Future<void> Initialize(AdParent parent) = 0;

  // Retrieves the most recent Future for Initialize().
  Future<void> GetInitializeLastResult();

  // Initiates queryInfo creation for the given ad format and request.
  virtual Future<QueryInfoResult> CreateQueryInfo(AdFormat format,
                                                  const AdRequest& request) = 0;

  // Initiates queryInfo creation for the given ad format, request and ad unit.
  virtual Future<QueryInfoResult> CreateQueryInfoWithAdUnit(
      AdFormat format, const AdRequest& request, const char* ad_unit_id) = 0;

  // Retrieves the most recent QueryInfo future for the given create function.
  Future<QueryInfoResult> CreateQueryInfoLastResult(QueryInfoFn fn);

  // Returns true if the QueryInfo has been initialized.
  virtual bool is_initialized() const = 0;

 protected:
  friend class firebase::gma::QueryInfo;
  friend class firebase::gma::GmaInternal;

  // Used by CreateInstance() to create an appropriate one for the current
  // platform.
  explicit QueryInfoInternal(QueryInfo* base);

  // A pointer back to the QueryInfo class that created us.
  QueryInfo* base_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_QUERY_INFO_INTERNAL_H_
