#include "firestore/src/android/blob_android.h"

#include "firestore/src/jni/array.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Array;
using jni::Class;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticMethod;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/Blob";
jclass g_class = nullptr;

Method<Object> kConstructor("<init>", "(Lcom/google/protobuf/ByteString;)V");
StaticMethod<BlobInternal> kFromBytes(
    "fromBytes", "([B)Lcom/google/firebase/firestore/Blob;");
Method<Array<uint8_t>> kToBytes("toBytes", "()[B");

}  // namespace

void BlobInternal::Initialize(jni::Loader& loader) {
  g_class = loader.LoadClass(kClass);
  loader.LoadAll(kConstructor, kFromBytes, kToBytes);
}

Class BlobInternal::GetClass() { return Class(g_class); }

Local<BlobInternal> BlobInternal::Create(Env& env, const uint8_t* value,
                                         size_t size) {
  Local<Array<uint8_t>> byte_array = env.NewArray<uint8_t>(size);
  env.SetArrayRegion(byte_array, 0, size, value);
  return env.Call(kFromBytes, byte_array);
}

Local<Array<uint8_t>> BlobInternal::ToBytes(Env& env) const {
  return env.Call(*this, kToBytes);
}

}  // namespace firestore
}  // namespace firebase
