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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_CONVERTER_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_CONVERTER_ANDROID_H_

#include <utility>
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

// The struct is just to make declaring `MakePublic` a friend easier (and
// future-proof in case more parameters are added to it).
struct ConverterImpl {
  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static PublicT MakePublicFromInternal(InternalT&& from) {
    auto* internal = new InternalT(std::forward<InternalT>(from));
    return PublicT(internal);
  }

  /** Makes a public type, taking ownership of the pointer. */
  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static PublicT MakePublicFromInternal(InternalT* from) {
    return PublicT(from);
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static PublicT MakePublicFromJava(jni::Env& env, const jni::Object& object) {
    if (!env.ok() || !object) return {};

    return PublicT(new InternalT(object));
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static PublicT MakePublicFromJava(jni::Env& env,
                                    FirestoreInternal* firestore,
                                    const jni::Object& object) {
    if (!env.ok() || !object) return {};

    return PublicT(new InternalT(firestore, object));
  }

  template <typename PublicT, typename InternalT = InternalType<PublicT>>
  static InternalT* GetInternal(PublicT& from) {
    // static_cast is required for CollectionReferenceInternal because its
    // internal_ is actually of type QueryInternal* (inherited from its base
    // class). Static casts of unrelated types will still fail so this will
    // still fail on mismatched public/internal types.
    return static_cast<InternalT*>(from.internal_);
  }
};

// MakePublic

template <typename PublicT, typename InternalT = InternalType<PublicT>>
PublicT MakePublic(InternalT* internal) {
  return ConverterImpl::MakePublicFromInternal<PublicT, InternalT>(internal);
}

template <typename PublicT, typename InternalT = InternalType<PublicT>>
PublicT MakePublic(jni::Env& env, const jni::Object& object) {
  return ConverterImpl::MakePublicFromJava<PublicT, InternalT>(env, object);
}

template <typename PublicT, typename InternalT = InternalType<PublicT>>
PublicT MakePublic(jni::Env& env,
                   FirestoreInternal* firestore,
                   const jni::Object& object) {
  return ConverterImpl::MakePublicFromJava<PublicT, InternalT>(env, firestore,
                                                               object);
}

// GetInternal

template <typename PublicT, typename InternalT = InternalType<PublicT>>
InternalT* GetInternal(PublicT* from) {
  if (!from) return nullptr;

  return ConverterImpl::GetInternal(*from);
}

template <typename PublicT, typename InternalT = InternalType<PublicT>>
InternalT* GetInternal(PublicT& from) {
  return ConverterImpl::GetInternal(from);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_CONVERTER_ANDROID_H_
