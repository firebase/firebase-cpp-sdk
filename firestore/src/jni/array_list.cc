#include "firestore/src/jni/array_list.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Constructor<ArrayList> kConstructor("()V");
Constructor<ArrayList> kConstructorWithSize("(I)V");

}  // namespace

void ArrayList::Initialize(Loader& loader) {
  loader.LoadFromExistingClass("java/util/ArrayList",
                               util::array_list::GetClass(), kConstructor,
                               kConstructorWithSize);
}

Local<ArrayList> ArrayList::Create(Env& env) { return env.New(kConstructor); }

Local<ArrayList> ArrayList::Create(Env& env, size_t size) {
  return env.New(kConstructorWithSize, size);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
