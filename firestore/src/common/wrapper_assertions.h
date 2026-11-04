// Copyright 2020 Google LLC
#ifndef FIREBASE_FIRESTORE_SRC_COMMON_WRAPPER_ASSERTIONS_H_
#define FIREBASE_FIRESTORE_SRC_COMMON_WRAPPER_ASSERTIONS_H_

#include <utility>
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore.h"
#include "gtest/gtest.h"

#if defined(__ANDROID__)
#include <jni.h>
#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/firestore_android.h"
#endif

namespace firebase::firestore {

#if defined(__ANDROID__)
App* GetApp();
#endif

class ListenerRegistrationInternal;

namespace testutil {

#if defined(__ANDROID__)

template <typename FirestoreTypeInternal>
FirestoreTypeInternal* NewInternal() {
  InitResult init_result;
  Firestore* firestore = Firestore::GetInstance(GetApp(), &init_result);
  EXPECT_EQ(kInitResultSuccess, init_result);
  jni::Env env;
  auto dummy = env.NewStringUtf("dummy");
  return new FirestoreTypeInternal(GetInternal(firestore), dummy);
}

template <>
FieldValueInternal* NewInternal<FieldValueInternal>();
template <>
ListenerRegistrationInternal* NewInternal<ListenerRegistrationInternal>();

template <typename FirestoreType, typename FirestoreTypeInternal = InternalType<FirestoreType>>
void AssertWrapperConstructionContract() {
  FirestoreType default_instance;
  EXPECT_EQ(nullptr, GetInternal(default_instance));

  auto* internal = NewInternal<FirestoreTypeInternal>();
  auto instance = MakePublic<FirestoreType>(internal);
  EXPECT_EQ(internal, GetInternal(instance));

  FirestoreType instance_copy(instance);
  EXPECT_NE(nullptr, GetInternal(instance_copy));
  EXPECT_NE(internal, GetInternal(instance_copy));

  FirestoreType instance_move(std::move(instance));
  EXPECT_EQ(internal, GetInternal(instance_move));
  EXPECT_EQ(nullptr, GetInternal(instance));
}

template <typename FirestoreType, typename FirestoreTypeInternal = InternalType<FirestoreType>>
void AssertWrapperAssignmentContract() {
  auto* internal = NewInternal<FirestoreTypeInternal>();
  auto instance = MakePublic<FirestoreType>(internal);
  FirestoreType instance_copy;
  instance_copy = instance;
  EXPECT_NE(nullptr, GetInternal(instance_copy));
  EXPECT_NE(internal, GetInternal(instance_copy));

  FirestoreType instance_move;
  instance_move = std::move(instance);
  EXPECT_EQ(internal, GetInternal(instance_move));
  EXPECT_EQ(nullptr, GetInternal(instance));
}

#else

template <typename FirestoreTypeInternal>
FirestoreTypeInternal* NewInternal() {
  return new FirestoreTypeInternal();
}

#endif

}  // namespace testutil
}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_WRAPPER_ASSERTIONS_H_
