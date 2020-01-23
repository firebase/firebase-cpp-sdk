#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_UTIL_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_UTIL_H_

#include <string>

namespace firebase {
namespace firestore {

// Returns a reference to an empty string. This is useful for functions that may
// need to return the reference while being in a state where they don't have
// a string to refer to.
const std::string& EmptyString();

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_UTIL_H_
