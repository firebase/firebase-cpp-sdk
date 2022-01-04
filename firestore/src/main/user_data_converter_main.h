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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_USER_DATA_CONVERTER_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_USER_DATA_CONVERTER_MAIN_H_

#include <unordered_set>
#include <utility>
#include <vector>

#include "Firestore/Protos/nanopb/google/firestore/v1/document.nanopb.h"
#include "Firestore/core/src/model/database_id.h"
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/nanopb/message.h"
#include "absl/types/optional.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

class SetOptions;

namespace core {
class ParsedSetData;
class ParsedUpdateData;
class ParseAccumulator;
class ParseContext;
}  // namespace core

class UserDataConverter {
 public:
  explicit UserDataConverter(const model::DatabaseId* database_id)
      : database_id_{database_id} {}

  /** Parse document data from a non-merge `SetData` call. */
  core::ParsedSetData ParseSetData(const MapFieldValue& input) const;

  /**
   * Parse document data from `SetData` call. Whether it's treated as a merge is
   * determined by the given `options`.
   */
  core::ParsedSetData ParseSetData(const MapFieldValue& input,
                                   const SetOptions& options) const;

  /** Parse update data from an `UpdateData` call. */
  core::ParsedUpdateData ParseUpdateData(const MapFieldValue& input) const;
  core::ParsedUpdateData ParseUpdateData(const MapFieldPathValue& input) const;

  /**
   * Parse a "query value" (e.g. value in a where filter or a value in a cursor
   * bound).
   */
  nanopb::Message<google_firestore_v1_Value> ParseQueryValue(
      const FieldValue& input, bool allow_arrays = false) const;

 private:
  using UpdateDataInput =
      std::vector<std::pair<model::FieldPath, const FieldValue*>>;

  /** Parse document data from a merge `SetData` call. */
  core::ParsedSetData ParseMergeData(
      const MapFieldValue& input,
      const absl::optional<std::unordered_set<FieldPath>>& field_mask =
          absl::nullopt) const;

  /**
   * Converts a given public C++ `FieldValue` into its internal equivalent.
   * If the value is a sentinel value, however, returns `nullopt`; the result of
   * the function in that case will be the side effect of modifying the given
   * `context`.
   */
  absl::optional<nanopb::Message<google_firestore_v1_Value>> ParseData(
      const FieldValue& input, core::ParseContext&& context) const;
  nanopb::Message<google_firestore_v1_Value> ParseArray(
      const std::vector<FieldValue>& input, core::ParseContext&& context) const;
  nanopb::Message<google_firestore_v1_Value> ParseMap(
      const MapFieldValue& input, core::ParseContext&& context) const;

  /**
   * "Parses" the provided sentinel `FieldValue`, adding any necessary
   * transforms to the field transforms on the given `context`.
   */
  void ParseSentinel(const FieldValue& input,
                     core::ParseContext&& context) const;

  /** Parses a scalar value (i.e. not a container or a sentinel). */
  nanopb::Message<google_firestore_v1_Value> ParseScalar(
      const FieldValue& input, core::ParseContext&& context) const;

  nanopb::Message<google_firestore_v1_ArrayValue> ParseArrayTransformElements(
      const FieldValue& value) const;

  // Storing `FieldValue`s as pointers in `UpdateDataInput` avoids copying them.
  // The objects must be valid for the duration of this `ParsedUpdateData` call.
  core::ParsedUpdateData ParseUpdateData(const UpdateDataInput& input) const;

  const model::DatabaseId* database_id_ = nullptr;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_USER_DATA_CONVERTER_MAIN_H_
