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
#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_BUNDLE_BUILDER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_BUNDLE_BUILDER_H_

#include <string>

namespace firebase {
namespace firestore {

/**
 * Builds a bundle from a template by replacing project id placeholders with
 * the given project id.
 */
std::string CreateBundle(const std::string& project_id);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_TESTS_UTIL_BUNDLE_BUILDER_H_
