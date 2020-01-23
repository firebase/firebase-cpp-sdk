#include "firestore/src/common/util.h"

namespace firebase {
namespace firestore {

const std::string& EmptyString() {
  static const std::string kEmptyString;
  return kEmptyString;
}

}  // namespace firestore
}  // namespace firebase
