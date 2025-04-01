#ifndef FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_FIELD_H_
#define FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_FIELD_H_

#include <string>
#include <memory>
#include "firebase/firestore/field_path.h"

namespace firebase {
namespace firestore {

/**
 * @brief Represents a field to be used in an aggregation operation.
 */
class AggregateField {
 public:
  /**
   * @brief Enum representing the type of aggregation.
   */
  enum class Type {
    kCount,
    kSum,
    // TODO: Add kAverage
  };

  // Constructor to initialize the AggregateField with a type and optional field path.
  AggregateField(Type type, const FieldPath& field_path = FieldPath());

  // Getter for the aggregation type.
  Type type() const;

  // Getter for the associated field path.
  const FieldPath& field_path() const;

 private:
  Type type_;               // The type of aggregation.
  FieldPath field_path_;    // The field path associated with the aggregation.
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_AGGREGATE_FIELD_H_
