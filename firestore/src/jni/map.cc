#include "firestore/src/jni/map.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/set.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Method<size_t> kSize("size", "()I");
Method<Object> kGet("get", "(Ljava/lang/Object;)Ljava/lang/Object;");
Method<Object> kPut("put",
                    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
Method<Object> kRemove("remove", "(Ljava/lang/Object;)Ljava/lang/Object;");
Method<Set> kKeySet("keySet", "()Ljava/util/Set;");
jclass g_clazz = nullptr;

}  // namespace

void Map::Initialize(Loader& loader) {
  g_clazz = util::map::GetClass();
  loader.LoadFromExistingClass("java/util/Map", g_clazz, kSize, kGet, kPut,
                               kRemove, kKeySet);
}

Class Map::GetClass() { return Class(g_clazz); }

size_t Map::Size(Env& env) const { return env.Call(*this, kSize); }

Local<Object> Map::Get(Env& env, const Object& key) {
  return env.Call(*this, kGet, key);
}

Local<Object> Map::Put(Env& env, const Object& key, const Object& value) {
  return env.Call(*this, kPut, key, value);
}

Local<Object> Map::Remove(Env& env, const Object& key) {
  return env.Call(*this, kRemove, key);
}

Local<Set> Map::KeySet(Env& env) { return env.Call(*this, kKeySet); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
