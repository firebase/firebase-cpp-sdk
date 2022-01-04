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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_UTIL_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_UTIL_ANDROID_H_

#include <vector>

#include "firestore/src/android/converter_android.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/list.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {

/**
 * Converts a java list of java type e.g. java.util.List<JavaType> to a C++
 * vector of equivalent type e.g. std::vector<PublicT>.
 */
template <typename PublicT, typename InternalT = InternalType<PublicT>>
std::vector<PublicT> MakePublicVector(jni::Env& env,
                                      FirestoreInternal* firestore,
                                      const jni::List& from) {
  size_t size = from.Size(env);
  std::vector<PublicT> result;
  result.reserve(size);

  for (int i = 0; i < size; ++i) {
    jni::Local<jni::Object> element = from.Get(env, i);

    // Avoid creating a partially valid public object on failure.
    if (!env.ok()) return {};

    // Use push_back because emplace_back requires a public constructor.
    result.push_back(MakePublic<PublicT>(env, firestore, element));
  }
  return result;
}

// Converts a MapFieldValue to a Java Map object that maps String to Object.
jni::Local<jni::HashMap> MakeJavaMap(jni::Env& env, const MapFieldValue& data);

/**
 * The result of parsing a `MapFieldPathValue` object into its equivalent
 * arguments, prepared for calling a Firestore Java `update` method. `update`
 * takes its first two arguments separate from a varargs array.
 *
 * An `UpdateFieldPathArgs` object is only valid as long as the
 * `MapFieldPathValue` object from which it is created is valid because these
 * are not new references. This is reflected in the fact that `first_value`
 * is not an owning reference to its `jni::Object`.
 */
struct UpdateFieldPathArgs {
  jni::Local<jni::Object> first_field;
  jni::Object first_value;
  jni::Local<jni::Array<jni::Object>> varargs;
};

/**
 * Creates the variadic parameters for a call to Java `update` from a C++
 * `MapFieldPathValue`. The result separates the first field and value because
 * Android Java API requires passing the first pair separately. The caller is
 * is responsible for verifying that `data` has at least one element.
 */
UpdateFieldPathArgs MakeUpdateFieldPathArgs(jni::Env& env,
                                            const MapFieldPathValue& data);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_UTIL_ANDROID_H_
