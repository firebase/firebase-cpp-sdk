#include "firestore/src/jni/list.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Method<Object> kGet("get", "(I)Ljava/lang/Object;");
Method<Object> kSet("set", "(ILjava/lang/Object;)Ljava/lang/Object;");

}  // namespace

void List::Initialize(Loader& loader) {
  loader.LoadFromExistingClass("java/util/List", util::list::GetClass(), kGet,
                               kSet);
}

Local<Object> List::Get(Env& env, size_t i) const {
  return env.Call(*this, kGet, i);
}

Local<Object> List::Set(Env& env, size_t i, const Object& object) {
  return env.Call(*this, kSet, i, object);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
