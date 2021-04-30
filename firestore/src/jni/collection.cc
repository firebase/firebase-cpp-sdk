#include "firestore/src/jni/collection.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/iterator.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClass[] = "java/util/Collection";
Method<bool> kAdd("add", "(Ljava/lang/Object;)Z");
Method<Iterator> kIterator("iterator", "()Ljava/util/Iterator;");
Method<size_t> kSize("size", "()I");

}  // namespace

void Collection::Initialize(Loader& loader) {
  loader.LoadClass(kClass, kAdd, kIterator, kSize);
}

bool Collection::Add(Env& env, const Object& object) {
  return env.Call(*this, kAdd, object);
}

Local<Iterator> Collection::Iterator(Env& env) {
  return env.Call(*this, kIterator);
}

size_t Collection::Size(Env& env) const { return env.Call(*this, kSize); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
