#include "firestore/src/jni/hash_map.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClass[] = "java/util/HashMap";
Constructor<HashMap> kConstructor("()V");

}  // namespace

void HashMap::Initialize(Loader& loader) {
  loader.LoadFromExistingClass(kClass, util::hash_map::GetClass(),
                               kConstructor);
}

Local<HashMap> HashMap::Create(Env& env) { return env.New(kConstructor); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
