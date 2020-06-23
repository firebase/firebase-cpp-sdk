// Copyright 2020 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_TESTS_FLEXBUFFER_MATCHER_H_
#define FIREBASE_APP_CLIENT_CPP_TESTS_FLEXBUFFER_MATCHER_H_

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "flatbuffers/flexbuffers.h"

// TODO(73494146): Check in EqualsFlexbuffer gmock matcher into the canonical
// Flatbuffer repository.
// Because pushing things to the Flatbuffers library is a multistep process, I'm
// including this for now so the tests can be built. Once this has been merged
// into flatbuffers, we can remove this implementation of it and use the one
// supplied by Flatbuffers.
//
// Checks the equality of two Flexbuffers. This checker ignores whether values
// are 'Indirect' and typed vectors are treated as plain vectors.
bool EqualsFlexbufferImpl(const flexbuffers::Reference& expected,
                          const flexbuffers::Reference& arg,
                          const std::string& location,
                          ::testing::MatchResultListener* result_listener);

bool EqualsFlexbufferImpl(const flexbuffers::Reference& expected,
                          const std::vector<uint8_t>& arg,
                          const std::string& location,
                          ::testing::MatchResultListener* result_listener);

bool EqualsFlexbufferImpl(const std::vector<uint8_t>& expected,
                          const flexbuffers::Reference& arg,
                          const std::string& location,
                          ::testing::MatchResultListener* result_listener);

bool EqualsFlexbufferImpl(const std::vector<uint8_t>& expected,
                          const std::vector<uint8_t>& arg,
                          const std::string& location,
                          ::testing::MatchResultListener* result_listener);

// TODO(73494146): Move this to Flabuffers.
MATCHER_P(EqualsFlexbuffer, expected, "") {
  return EqualsFlexbufferImpl(expected, arg, "", result_listener);
}

#endif  // FIREBASE_APP_CLIENT_CPP_TESTS_FLEXBUFFER_MATCHER_H_
