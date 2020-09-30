#include "firestore/src/jni/throwable.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace jni {

std::string Throwable::GetMessage(Env& env) const {
  ExceptionClearGuard block(env);
  return util::GetMessageFromException(env.get(), object_);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
