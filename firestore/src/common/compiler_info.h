// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_COMPILER_INFO_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_COMPILER_INFO_H_

#include <string>

namespace firebase {
namespace firestore {

// Returns a string describing the compiler version and settings in the
// following format:
//
//   <CompilerId>-<CompilerVersion>-<CompilerFeatures>-<LanguageVersion>-<StandardLibraryVersion>
//
// e.g. "AppleClang-11.0.3.11030032-ex-2011-libcpp".
//
// The format is based on what is used by Cloud C++ libraries:
// https://github.com/googleapis/google-cloud-cpp/blob/211006b86c841f2226fedf2f7ae6ced482aa2cc0/google/cloud/internal/api_client_header.cc#L23-L29
// with the addition of <StandardLibraryVersion> (e.g. "libcpp").
std::string GetFullCompilerInfo();

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_COMPILER_INFO_H_
