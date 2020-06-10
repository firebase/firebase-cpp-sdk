/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_VARIANT_UTIL_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_VARIANT_UTIL_H_

#include <string>

#include "app/src/include/firebase/variant.h"
#include "flatbuffers/flexbuffers.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace util {

// Convert from a JSON string to a Variant.
Variant JsonToVariant(const char* json);

// Converts a Variant to a JSON string.
std::string VariantToJson(const Variant& variant);
std::string VariantToJson(const Variant& variant, bool prettyPrint);

// Converts an std::map<Variant, Variant> to Json
std::string StdMapToJson(const std::map<Variant, Variant>& map);

// Converts an std::vector<Variant> to Json
std::string StdVectorToJson(const std::vector<Variant>& vector);

// Convert from a Flexbuffer reference to a Variant.
Variant FlexbufferToVariant(const flexbuffers::Reference& ref);

// Convert from a Flexbuffer map to a Variant.
Variant FlexbufferMapToVariant(const flexbuffers::Map& map);

// Convert from a Flexbuffer vector to a Variant.
Variant FlexbufferVectorToVariant(const flexbuffers::Vector& vector);

// Convert from a Variant to a Flexbuffer buffer.
std::vector<uint8_t> VariantToFlexbuffer(const Variant& variant);

// Convert from a Variant map to a Flexbuffer buffer.
std::vector<uint8_t> VariantMapToFlexbuffer(
    const std::map<Variant, Variant>& map);

// Convert from a Variant vector to a Flexbuffer buffer.
std::vector<uint8_t> VariantVectorToFlexbuffer(
    const std::vector<Variant>& vector);

// Convert from a variant to a Flexbuffer using the given flexbuffer Builder.
// Returns true on success, false otherwise.
bool VariantToFlexbuffer(const Variant& variant, flexbuffers::Builder* fbb);

// Convert from a variant to a Flexbuffer using the given flexbuffer Builder.
// Returns true on success, false otherwise.
bool VariantMapToFlexbuffer(const std::map<Variant, Variant>& map,
                            flexbuffers::Builder* fbb);

// Convert from a variant to a Flexbuffer using the given flexbuffer Builder.
// Returns true on success, false otherwise.
bool VariantVectorToFlexbuffer(const std::vector<Variant>& vector,
                               flexbuffers::Builder* fbb);

}  // namespace util
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_VARIANT_UTIL_H_
