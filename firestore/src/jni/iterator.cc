#include "firestore/src/jni/iterator.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClass[] = "java/util/Iterator";
Method<bool> kHasNext("hasNext", "()Z");
Method<Object> kNext("next", "()Ljava/lang/Object;");

}  // namespace

void Iterator::Initialize(Loader& loader) {
  loader.LoadFromExistingClass(kClass, util::iterator::GetClass(), kHasNext,
                               kNext);
}

bool Iterator::HasNext(Env& env) const { return env.Call(*this, kHasNext); }

Local<Object> Iterator::Next(Env& env) { return env.Call(*this, kNext); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
