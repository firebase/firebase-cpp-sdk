/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/android/field_path_android.h"

#include "firestore/src/android/field_path_portable.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore {
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
  if (path.internal_ == nullptr || path.internal_->size() == 0) {
    SimpleThrowInvalidArgument(
        "Invalid field path. Provided path must not be empty.");
    Local<Array<String>> empty_args =
        env.NewArray<String>(0, String::GetClass());
    return env.Call(kOf, empty_args);
  }
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

}  // namespace firebase::firestore
