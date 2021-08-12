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

#include "firestore/src/common/wrapper_assertions.h"

#if defined(__ANDROID__)
#include "firestore/src/android/field_value_android.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {
namespace testutil {

#if defined(__ANDROID__)

template <>
FieldValueInternal* NewInternal<FieldValueInternal>() {
  InitResult init_result;
  Firestore* firestore = Firestore::GetInstance(GetApp(), &init_result);
  (void)firestore;

  EXPECT_EQ(kInitResultSuccess, init_result);
  jni::Env env;

  // We use a Java String object as a dummy to create the internal type. There
  // is no generic way to create an actual Java object of internal type. But
  // since we are not actually do any JNI call to the Java object, any Java
  // object is just as good. We cannot pass in nullptr since most of the wrapper
  // does not allow to wrap nullptr object.
  auto dummy = env.NewStringUtf("dummy");
  return new FieldValueInternal(dummy);
}

template <>
ListenerRegistrationInternal* NewInternal<ListenerRegistrationInternal>() {
  return nullptr;
}

#endif  // defined(__ANDROID__)

}  // namespace testutil
}  // namespace firestore
}  // namespace firebase
