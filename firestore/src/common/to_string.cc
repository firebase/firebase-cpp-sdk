#include "firestore/src/common/to_string.h"

#include <ostream>

#include "firestore/src/include/firebase/firestore/field_value.h"

namespace firebase {
namespace firestore {

// TODO(varconst): optional indentation.
std::string ToString(const MapFieldValue& map) {
  std::string result = "{";

  bool is_first = true;
  for (const auto& kv : map) {
    if (!is_first) {
      result += ", ";
    }
    is_first = false;

    result += kv.first;
    result += ": ";
    result += kv.second.ToString();
  }

  result += '}';
  return result;
}

std::ostream& operator<<(std::ostream& out, const MapFieldValue& value) {
  return out << ToString(value);
}

}  // namespace firestore
}  // namespace firebase
