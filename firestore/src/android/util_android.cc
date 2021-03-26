#include "firestore/src/android/util_android.h"

#include "app/src/assert.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"

namespace firebase {
namespace firestore {

using jni::Array;
using jni::Env;
using jni::HashMap;
using jni::Local;
using jni::Object;
using jni::String;

Local<HashMap> MakeJavaMap(Env& env, const MapFieldValue& data) {
  Local<HashMap> result = HashMap::Create(env);
  for (const auto& kv : data) {
    Local<String> key = env.NewStringUtf(kv.first);
    const Object& value = ToJava(kv.second);
    result.Put(env, key, value);
  }
  return result;
}

UpdateFieldPathArgs MakeUpdateFieldPathArgs(Env& env,
                                            const MapFieldPathValue& data) {
  auto iter = data.begin();
  auto end = data.end();
  FIREBASE_DEV_ASSERT_MESSAGE(iter != end, "data must be non-empty");

  Local<Object> first_field = FieldPathConverter::Create(env, iter->first);
  const Object& first_value = ToJava(iter->second);
  ++iter;

  const auto size = std::distance(iter, end) * 2;
  Local<Array<Object>> varargs = env.NewArray(size, Object::GetClass());

  int index = 0;
  for (; iter != end; ++iter) {
    Local<Object> field = FieldPathConverter::Create(env, iter->first);
    const Object& value = ToJava(iter->second);

    varargs.Set(env, index++, field);
    varargs.Set(env, index++, value);
  }

  return UpdateFieldPathArgs{Move(first_field), first_value, Move(varargs)};
}

}  // namespace firestore
}  // namespace firebase
