#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_USER_DATA_CONVERTER_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_USER_DATA_CONVERTER_IOS_H_

#include <utility>
#include <vector>

#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "absl/types/optional.h"
#include "Firestore/core/src/model/database_id.h"
#include "Firestore/core/src/model/field_path.h"
#include "Firestore/core/src/model/field_value.h"

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
  model::FieldValue ParseQueryValue(const FieldValue& input,
                                    bool allow_arrays = false) const;

 private:
  using UpdateDataInput =
      std::vector<std::pair<model::FieldPath, const FieldValue*>>;

  /** Parse document data from a merge `SetData` call. */
  core::ParsedSetData ParseMergeData(
      const MapFieldValue& input,
      const absl::optional<std::vector<FieldPath>>& field_mask =
          absl::nullopt) const;

  /**
   * Converts a given public C++ `FieldValue` into its internal equivalent.
   * If the value is a sentinel value, however, returns `nullopt`; the result of
   * the function in that case will be the side effect of modifying the given
   * `context`.
   */
  absl::optional<model::FieldValue> ParseData(
      const FieldValue& input, core::ParseContext&& context) const;
  model::FieldValue::Array ParseArray(const std::vector<FieldValue>& input,
                                      core::ParseContext&& context) const;
  model::ObjectValue ParseMap(const MapFieldValue& input,
                              core::ParseContext&& context) const;

  /**
   * "Parses" the provided sentinel `FieldValue`, adding any necessary
   * transforms to the field transforms on the given `context`.
   */
  void ParseSentinel(const FieldValue& input,
                     core::ParseContext&& context) const;

  /** Parses a scalar value (i.e. not a container or a sentinel). */
  model::FieldValue ParseScalar(const FieldValue& input,
                                core::ParseContext&& context) const;

  model::FieldValue::Array ParseArrayTransformElements(
      const FieldValue& value) const;

  // Storing `FieldValue`s as pointers in `UpdateDataInput` avoids copying them.
  // The objects must be valid for the duration of this `ParsedUpdateData` call.
  core::ParsedUpdateData ParseUpdateData(const UpdateDataInput& input) const;

  const model::DatabaseId* database_id_ = nullptr;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_USER_DATA_CONVERTER_IOS_H_
