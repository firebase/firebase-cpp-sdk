#include "firestore/src/jni/string.h"

#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace jni {

std::string String::ToString(Env& env) const {
  size_t len = env.GetStringUtfLength(*this);
  return env.GetStringUtfRegion(*this, 0, len);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
