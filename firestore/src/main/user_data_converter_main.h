// Copyright 2021 Google LLC
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

namespace firebase::firestore {

class SetOptions;
namespace core {
class ParsedSetData;
class ParsedUpdateData;
class ParseAccumulator;
class ParseContext;
}  // namespace core

class UserDataConverter {
 public:
  explicit UserDataConverter(const model::DatabaseId* database_id) : database_id_{database_id} {}

  core::ParsedSetData ParseSetData(const MapFieldValue& input) const;
  core::ParsedSetData ParseSetData(const MapFieldValue& input, const SetOptions& options) const;
  core::ParsedUpdateData ParseUpdateData(const MapFieldValue& input) const;
  core::ParsedUpdateData ParseUpdateData(const MapFieldPathValue& input) const;
  nanopb::Message<google_firestore_v1_Value> ParseQueryValue(
      const FieldValue& input, bool allow_arrays = false) const;

 private:
  using UpdateDataInput = std::vector<std::pair<model::FieldPath, const FieldValue*>>;

  core::ParsedSetData ParseMergeData(
      const MapFieldValue& input,
      const absl::optional<std::unordered_set<FieldPath>>& field_mask = absl::nullopt) const;
  absl::optional<nanopb::Message<google_firestore_v1_Value>> ParseData(
      const FieldValue& input, core::ParseContext&& context) const;
  nanopb::Message<google_firestore_v1_Value> ParseArray(
      const std::vector<FieldValue>& input, core::ParseContext&& context) const;
  nanopb::Message<google_firestore_v1_Value> ParseMap(
      const MapFieldValue& input, core::ParseContext&& context) const;
  void ParseSentinel(const FieldValue& input, core::ParseContext&& context) const;
  nanopb::Message<google_firestore_v1_Value> ParseScalar(
      const FieldValue& input, core::ParseContext&& context) const;
  nanopb::Message<google_firestore_v1_ArrayValue> ParseArrayTransformElements(
      const FieldValue& value) const;
  core::ParsedUpdateData ParseUpdateData(const UpdateDataInput& input) const;

  const model::DatabaseId* database_id_ = nullptr;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_USER_DATA_CONVERTER_MAIN_H_
