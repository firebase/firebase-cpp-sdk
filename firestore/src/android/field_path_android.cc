#include "firestore/src/android/field_path_android.h"

#include "firestore/src/jni/array.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

#if defined(__ANDROID__)
#include "firestore/src/android/field_path_portable.h"
#else  // defined(__ANDROID__)
#include "Firestore/core/src/model/field_path.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {
namespace {

using jni::Array;
using jni::Env;
using jni::Local;
using jni::Object;
using jni::StaticMethod;
using jni::String;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/FieldPath";
StaticMethod<Object> kOf(
    "of", "([Ljava/lang/String;)Lcom/google/firebase/firestore/FieldPath;");
StaticMethod<Object> kDocumentId("documentId",
                                 "()Lcom/google/firebase/firestore/FieldPath;");

}  // namespace

void FieldPathConverter::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kOf, kDocumentId);
}

Local<Object> FieldPathConverter::Create(Env& env, const FieldPath& path) {
  FieldPath::FieldPathInternal& internal = *path.internal_;

  // If the path is key (i.e. __name__).
  if (internal.IsKeyFieldPath()) {
    return env.Call(kDocumentId);
  }

  // Prepare call arguments.
  size_t size = internal.size();
  Local<Array<String>> args = env.NewArray<String>(size, String::GetClass());
  for (size_t i = 0; i < size; ++i) {
    args.Set(env, i, env.NewStringUtf(internal[i]));
  }

  return env.Call(kOf, args);
}

}  // namespace firestore
}  // namespace firebase
