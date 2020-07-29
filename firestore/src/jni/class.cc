#include "firestore/src/jni/class.h"

#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace jni {

std::string Class::GetName(Env& env) const {
  jmethodID method = env.GetMethodId(*this, "name", "()Ljava/lang/String;");
  return env.Call<String>(*this, method).ToString(env);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
