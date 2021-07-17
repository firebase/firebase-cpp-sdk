// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_SET_OPTIONS_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_SET_OPTIONS_MAIN_H_

#include <utility>

#include "firestore/src/include/firebase/firestore/set_options.h"

namespace firebase {
namespace firestore {

/**
 * A wrapper over the public `SetOptions` class that allows getting access to
 * the values stored (the public class only allows setting options, not
 * retrieving them).
 */
class SetOptionsInternal {
 public:
  using Type = SetOptions::Type;

  explicit SetOptionsInternal(SetOptions options)
      : options_{std::move(options)} {}

  Type type() const { return options_.type_; }
  const std::vector<FieldPath>& field_mask() const { return options_.fields_; }

 private:
  SetOptions options_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_SET_OPTIONS_MAIN_H_
