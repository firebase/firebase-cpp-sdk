#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_TO_STRING_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_TO_STRING_H_

#include <iosfwd>
#include <string>

#include "firestore/src/include/firebase/firestore/map_field_value.h"

// This file contains string converters that aren't exposed publicly but are
// used by more than one of the public string converters in their
// implementation.

namespace firebase {
namespace firestore {

// Returns a string representation of the given `MapFieldValue` for
// logging/debugging purposes.
//
// Note: the exact string representation is unspecified and subject to change;
// don't rely on the format of the string.
std::string ToString(const MapFieldValue& value);

// Outputs the string representation of the given `MapFieldValue` to the given
// stream.
//
// See `ToString` above for comments on the representation format.
//
std::ostream& operator<<(std::ostream& out, const MapFieldValue& value);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_TO_STRING_H_
