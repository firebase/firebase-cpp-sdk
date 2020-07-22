#include "firestore/src/jni/string.h"

#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace jni {

std::string String::ToString(Env& env) const {
  jstring str = get();
  size_t len = env.GetStringUtfLength(str);
  return env.GetStringUtfRegion(str, 0, len);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
