// Copyright 2018 Google LLC
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

#include "database/src/desktop/query_params_comparator.h"

#include <cassert>
#include <cstdint>

#include "app/src/assert.h"
#include "app/src/util.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

const char QueryParamsComparator::kMinKey[] = "[MIN_KEY]";
const char QueryParamsComparator::kMaxKey[] = "[MAX_KEY]";

// These values are used as sentinel values since the database will never send
// down values with these types. These are never compared to other variants
// directly - they should only be used with the QueryParamsComparator, which
// uses the Variant::type() to do comparisons.
static const Variant kMinVariant = Variant::FromStaticBlob(  // NOLINT
    QueryParamsComparator::kMinKey, sizeof(QueryParamsComparator::kMinKey));
static const Variant kMaxVariant = Variant::FromStaticBlob(  // NOLINT
    QueryParamsComparator::kMaxKey, sizeof(QueryParamsComparator::kMaxKey));

const std::pair<Variant, Variant> QueryParamsComparator::kMinNode =  // NOLINT
    std::make_pair(QueryParamsComparator::kMinKey, kMinVariant);
const std::pair<Variant, Variant> QueryParamsComparator::kMaxNode =  // NOLINT
    std::make_pair(QueryParamsComparator::kMaxKey, kMaxVariant);

static int VariantIsSentinel(const Variant& key, const Variant& value) {
  if (key == QueryParamsComparator::kMinKey && value == kMinVariant) {
    return -1;
  } else if (key == QueryParamsComparator::kMaxKey && value == kMaxVariant) {
    return 1;
  } else {
    return 0;
  }
}

int QueryParamsComparator::Compare(const Variant& key_a, const Variant& value_a,
                                   const Variant& key_b,
                                   const Variant& value_b) const {
  assert(key_a.is_string() || key_a.is_int64());
  assert(key_b.is_string() || key_b.is_int64());

  // First check if either of our nodes is the special min or max sentinel
  // value. If that's the case, we can short circuit the rest of the comparison.
  int min_max_a = VariantIsSentinel(key_a, value_a);
  int min_max_b = VariantIsSentinel(key_b, value_b);
  if (min_max_a != min_max_b) {
    return min_max_a - min_max_b;
  }

  switch (query_params_->order_by) {
    case QueryParams::kOrderByPriority: {
      int result = ComparePriorities(value_a, value_b);
      if (result == 0) {
        result = CompareKeys(key_a, key_b);
      }
      return result;
    }
    case QueryParams::kOrderByChild: {
      int result = CompareChildren(value_a, value_b);
      if (result == 0) {
        result = CompareKeys(key_a, key_b);
      }
      return result;
    }
    case QueryParams::kOrderByKey: {
      return CompareKeys(key_a, key_b);
    }
    case QueryParams::kOrderByValue: {
      int result = CompareValues(value_a, value_b);
      if (result == 0) {
        result = CompareKeys(key_a, key_b);
      }
      return result;
    }
  }
  FIREBASE_DEV_ASSERT_MESSAGE(false, "Invalid QueryParams::OrderBy");
  return 0;
}

int QueryParamsComparator::ComparePriorities(const Variant& value_a,
                                             const Variant& value_b) {
  const Variant& priority_a = GetVariantPriority(value_a);
  const Variant& priority_b = GetVariantPriority(value_b);
  // Priority comparisons follow the same rules as values.
  return CompareValues(priority_a, priority_b);
}

int QueryParamsComparator::CompareChildren(const Variant& value_a,
                                           const Variant& value_b) const {
  const Path path(query_params_->order_by_child);
  const Variant& descendant_a = VariantGetChild(&value_a, path);
  const Variant& descendant_b = VariantGetChild(&value_b, path);
  // Child comparisons follow the same rules as values.
  return CompareValues(descendant_a, descendant_b);
}

int QueryParamsComparator::CompareKeys(const Variant& key_a,
                                       const Variant& key_b) {
  if (key_a == key_b) {
    return 0;
  } else if (key_a == kMinKey || key_b == kMaxKey) {
    return -1;
  } else if (key_b == kMinKey || key_a == kMaxKey) {
    return 1;
  } else if (key_a.is_int64()) {
    if (key_b.is_int64()) {
      return key_b.int64_value() - key_a.int64_value();
    } else {
      return -1;
    }
  } else if (key_b.is_int64()) {
    return 1;
  } else {
    return strcmp(key_a.string_value(), key_b.string_value());
  }
}

int QueryParamsComparator::CompareValues(const Variant& variant_a,
                                         const Variant& variant_b) {
  const Variant* value_a = GetVariantValue(&variant_a);
  const Variant* value_b = GetVariantValue(&variant_b);

  // The order of this enum matters. This order matches the order that RTDB
  // organizes nodes in if they are different types.
  enum Precedence {
    kPrecedenceFirst,
    kPrecedenceNull,
    kPrecedenceBoolean,
    kPrecedenceNumber,
    kPrecedenceString,
    kPrecedenceMap,
    kPrecedenceLast,
    kPrecedenceSentinel,
    kPrecedenceError,
  };
  // Map the precedence to the equivalent Variant types.
  // StaticBlob and MutableBlob values get special treatment here - they are
  // used as a speical sentinel values that will always be considered the first
  // or last element respectively. The server will never send down a Blob, so it
  // can be savely used here with a special meaning.
  static const Precedence kPrecedenceLookupTable[] = {
      kPrecedenceNull,      // kTypeNull
      kPrecedenceNumber,    // kTypeInt64
      kPrecedenceNumber,    // kTypeDouble
      kPrecedenceBoolean,   // kTypeBool
      kPrecedenceString,    // kTypeStaticString
      kPrecedenceString,    // kTypeMutableString
      kPrecedenceError,     // kTypeVector
      kPrecedenceMap,       // kTypeMap
      kPrecedenceSentinel,  // kTypeStaticBlob
      kPrecedenceError,     // kTypeMutableBlob
  };

  Variant::Type type_a = value_a->type();
  Variant::Type type_b = value_b->type();
  Precedence precedence_a = kPrecedenceLookupTable[type_a];
  Precedence precedence_b = kPrecedenceLookupTable[type_b];

  // If we encounted a special sentinel value, figure out what we actually want
  // the precendence to be.
  if (precedence_a == kPrecedenceSentinel) {
    assert(*value_a == kMinVariant || *value_a == kMaxVariant);
    precedence_a =
        (*value_a == kMinVariant) ? kPrecedenceFirst : kPrecedenceLast;
  }
  if (precedence_b == kPrecedenceSentinel) {
    assert(*value_b == kMinVariant || *value_b == kMaxVariant);
    precedence_b =
        (*value_b == kMinVariant) ? kPrecedenceFirst : kPrecedenceLast;
  }

  // Values coming down from the server should never contain blobs or vectors.
  assert(precedence_a != kPrecedenceError);
  assert(precedence_b != kPrecedenceError);

  // If we have different priorities we don't need to compare the values
  // themselves. Just return the difference between the priorities.
  if (precedence_a != precedence_b) {
    return precedence_a - precedence_b;
  }

  // If the priority is the same we need to compare the value of the types.
  switch (precedence_a) {
    case kPrecedenceFirst: {
      // Two first elements are equal. Once can't be more first than the other.
      return 0;
    }
    case kPrecedenceNull: {
      // Null types are always equal to other null types.
      return 0;
    }
    case kPrecedenceBoolean: {
      return (value_a->bool_value() == value_b->bool_value())
                 ? 0
                 : (value_a->bool_value() ? 1 : -1);
    }
    case kPrecedenceNumber: {
      // If they're both integers.
      if (type_a == Variant::kTypeInt64 && type_b == Variant::kTypeInt64) {
        int64_t int64_a = value_a->int64_value();
        int64_t int64_b = value_b->int64_value();

        if (int64_a < int64_b) return -1;
        if (int64_a > int64_b) return 1;
        return 0;
      }

      // At least one of them is a double, so we treat them both as doubles.
      // TODO(amablue): Technically, this comparison is imperfect when comparing
      // certain values int64 values that can't be perfectly cast to doubles.
      // Look into improving this comparison. This is good enough for now
      // though, since it's what the Android imlementation does.
      assert(type_a == Variant::kTypeDouble || type_b == Variant::kTypeDouble);
      double double_a = (type_a == Variant::kTypeDouble)
                            ? value_a->double_value()
                            : static_cast<double>(value_a->int64_value());
      double double_b = (type_b == Variant::kTypeDouble)
                            ? value_b->double_value()
                            : static_cast<double>(value_b->int64_value());

      if (double_a < double_b) return -1;
      if (double_a > double_b) return 1;
      return 0;
    }
    case kPrecedenceString: {
      return strcmp(value_a->string_value(), value_b->string_value());
    }
    case kPrecedenceMap: {
      // Maps are not compared against each other. Treat them as equal.
      return 0;
    }
    case kPrecedenceLast: {
      // Two last elements are equal. Once can't be more last than the other.
      return 0;
    }
    case kPrecedenceSentinel:
      FIREBASE_CASE_FALLTHROUGH;
    case kPrecedenceError: {
      // Assert for this condition should be caught above, but it's included
      // here for completeness sake (and lint).
      assert(0);
    }
  }
  return 0;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
