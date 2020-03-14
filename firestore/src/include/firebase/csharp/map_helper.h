#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_MAP_HELPER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_MAP_HELPER_H_

#include <string>
#include <vector>

#include "app/meta/move.h"
#include "firebase/firestore/field_path.h"
#include "firebase/firestore/field_value.h"
#include "firebase/firestore/map_field_value.h"

namespace firebase {
namespace firestore {
namespace csharp {

// FieldValue is converted to FieldValueInternal of internal access, which
// makes it impossible to use standard containers. What's more,
// MapFieldValue type depends on compilation settings. Providing the helper
// here, we provide a more transparent layer for C# calls.

// We cannot call std::vector<FieldValue>::foo() in C# since it is not
// exposed via the .dll interface as SWIG cannot convert it. Here we
// essentially define a function, which SWIG can convert and thus expose it
// via the .dll. So C# code can call it.

// It is OK to re-construct a vector of strings since those strings can be
// re-used by the C# code. This way is more clean than to expose an
// iterator.
inline std::vector<std::string> map_fv_keys(const MapFieldValue& self) {
  std::vector<std::string> result;
  result.reserve(self.size());
  for (const auto& kv : self) {
    result.push_back(kv.first);
  }
  return result;
}

inline const FieldValue& map_fv_get(const MapFieldValue& self,
                                    const std::string& key) {
  return self.at(key);
}

inline MapFieldValue map_fv_create() { return MapFieldValue{}; }

inline void map_fv_set(MapFieldValue* self, const std::string& key,
                       FieldValue value) {
  // This could be either a move-assignment or a normal assignment.
  (*self)[key] = firebase::Move(value);
}

inline MapFieldPathValue map_fpv_create() { return MapFieldPathValue{}; }

inline void map_set(MapFieldPathValue* self, FieldPath key, FieldValue value) {
  // These could be either move or copy.
  (*self)[firebase::Move(key)] = firebase::Move(value);
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_MAP_HELPER_H_
