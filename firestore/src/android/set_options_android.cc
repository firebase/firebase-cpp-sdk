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

#include "firestore/src/android/set_options_android.h"

#include <utility>

#include "app/src/assert.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/jni/array_list.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::ArrayList;
using jni::Env;
using jni::Local;
using jni::Object;
using jni::StaticField;
using jni::StaticMethod;

using Type = SetOptions::Type;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/SetOptions";

StaticMethod<Object> kMerge("merge",
                            "()Lcom/google/firebase/firestore/SetOptions;");
StaticMethod<Object> kMergeFieldPaths(
    "mergeFieldPaths",
    "(Ljava/util/List;)Lcom/google/firebase/firestore/SetOptions;");

StaticField<Object> kOverwrite("OVERWRITE",
                               "Lcom/google/firebase/firestore/SetOptions;");

}  // namespace

void SetOptionsInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kMerge, kMergeFieldPaths, kOverwrite);
}

Local<Object> SetOptionsInternal::Create(Env& env,
                                         const SetOptions& set_options) {
  switch (set_options.type_) {
    case Type::kOverwrite:
      return env.Get(kOverwrite);
    case Type::kMergeAll:
      return env.Call(kMerge);
    case Type::kMergeSpecific:
      // Do below this switch.
      break;
    default:
      FIREBASE_ASSERT_MESSAGE(false, "Unknown SetOptions type.");
      return {};
  }

  // Now we deal with options to merge specific fields.
  Local<ArrayList> fields = ArrayList::Create(env);
  for (const FieldPath& field : set_options.fields_) {
    fields.Add(env, FieldPathConverter::Create(env, field));
  }

  return env.Call(kMergeFieldPaths, fields);
}

}  // namespace firestore
}  // namespace firebase
