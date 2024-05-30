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

#ifndef FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_INTERNAL_QUERY_INFO_H_
#define FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_INTERNAL_QUERY_INFO_H_

#include <string>

#include "firebase/future.h"
#include "firebase/gma/types.h"

// Doxygen breaks trying to parse this file, and since it is internal logic,
// it doesn't need to be included in the generated documentation.
#ifndef DOXYGEN

namespace firebase {
namespace gma {

namespace internal {
// Forward declaration for platform-specific data, implemented in each library.
class QueryInfoInternal;
}  // namespace internal

class GmaInternal;
class QueryInfoResult;

class QueryInfo {
 public:
  QueryInfo();
  ~QueryInfo();

  /// Initialize the QueryInfo object.
  /// parent: The platform-specific application context.
  Future<void> Initialize(AdParent parent);

  /// Returns a Future containing the status of the last call to
  /// Initialize.
  Future<void> InitializeLastResult() const;

  /// Begins an asynchronous request for creating a query info string.
  ///
  /// format: The format of the ad for which the query info is being created.
  /// request: An AdRequest struct with information about the request
  ///                    to be made (such as targeting info).
  Future<QueryInfoResult> CreateQueryInfo(AdFormat format,
                                          const AdRequest& request);

  /// Returns a Future containing the status of the last call to
  /// CreateQueryInfo.
  Future<QueryInfoResult> CreateQueryInfoLastResult() const;

  /// Begins an asynchronous request for creating a query info string.
  ///
  /// format: The format of the ad for which the query info is being created.
  /// request: An AdRequest struct with information about the request
  ///                    to be made (such as targeting info).
  /// ad_unit_id: The ad unit ID to use in loading the ad.
  Future<QueryInfoResult> CreateQueryInfoWithAdUnit(AdFormat format,
                                                    const AdRequest& request,
                                                    const char* ad_unit_id);

  /// Returns a Future containing the status of the last call to
  /// CreateQueryInfoWithAdUnit.
  Future<QueryInfoResult> CreateQueryInfoWithAdUnitLastResult() const;

 private:
  // An internal, platform-specific implementation object that this class uses
  // to interact with the Google Mobile Ads SDKs for iOS and Android.
  internal::QueryInfoInternal* internal_;
};

/// Information about the result of a create queryInfo operation.
class QueryInfoResult {
 public:
  /// Default Constructor.
  QueryInfoResult();

  /// Destructor.
  virtual ~QueryInfoResult();

  /// Returns true if the operation was successful.
  bool is_successful() const;

  /// If the QueryInfoResult::is_successful() returned false, then the
  /// string returned via this method will contain no contextual
  /// information.
  const std::string& query_info() const;

 private:
  friend class GmaInternal;

  /// Constructor invoked upon successful query info generation.
  explicit QueryInfoResult(const std::string& query_info);

  /// Denotes if the QueryInfoResult represents a success or an error.
  bool is_successful_;

  /// Contains the full query info string.
  std::string query_info_;
};

}  // namespace gma
}  // namespace firebase

#endif  // DOXYGEN

#endif  // FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_INTERNAL_QUERY_INFO_H_
