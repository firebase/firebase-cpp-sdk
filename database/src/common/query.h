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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_QUERY_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_QUERY_H_

namespace firebase {
namespace database {
namespace internal {

// Future functions.
enum kQueryFn {
  kQueryFnGetValue = 0,
  kQueryFnCount,
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_COMMON_QUERY_H_
