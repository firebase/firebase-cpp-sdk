#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_WRAPPER_ASSERTIONS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_WRAPPER_ASSERTIONS_H_

#include "firestore/src/include/firebase/firestore.h"
#if defined(__ANDROID__)
#include <jni.h>

#include "firestore/src/android/firestore_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/firestore_stub.h"
#endif  // defined(__ANDROID__)

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

#if defined(__ANDROID__)
App* GetApp();
#endif  // defined(__ANDROID__)

class ListenerRegistrationInternal;

namespace testutil {

#if defined(__ANDROID__)

template <typename FirestoreTypeInternal>
FirestoreTypeInternal* NewInternal() {
  InitResult init_result;
  Firestore* firestore = Firestore::GetInstance(GetApp(), &init_result);
  EXPECT_EQ(kInitResultSuccess, init_result);
  JNIEnv* env = firestore->app()->GetJNIEnv();

  // We use a Java String object as a dummy to create the internal type. There
  // is no generic way to create an actual Java object of internal type. But
  // since we are not actually do any JNI call to the Java object, any Java
  // object is just as good. We cannot pass in nullptr since most of the wrapper
  // does not allow to wrap nullptr object.
  jobject dummy = env->NewStringUTF("dummy");
  FirestoreTypeInternal* result = new FirestoreTypeInternal{
      FirestoreInternal::Internal<FirestoreInternal>(*firestore), dummy};
  env->DeleteLocalRef(dummy);
  util::CheckAndClearJniExceptions(env);

  return result;
}

// It is technically complicated to create a true ListenerRegistrationInternal.
// All ListenerRegistrationInternal are owned by FirestoreInternal and require
// registered with the native SDK (i.e. using dummy Java object is infeasible).
// So we just returns a nullptr for test.
template <>
ListenerRegistrationInternal* NewInternal<ListenerRegistrationInternal>();

#else  // defined(__ANDROID__)

template <typename FirestoreTypeInternal>
FirestoreTypeInternal* NewInternal() {
  return new FirestoreTypeInternal();
}

#endif  // defined(__ANDROID__)

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)
// A helper that tests the common contract of the construction logic of
// Firestore wrapper class:
//   * The default constructed FirestoreType has the internal_ set to nullptr;
//   * FirestoreType constructed from FirestoreTypeInternal has the internal_
//     set to the passed in FirestoreTypeInternal instance;
//   * FirestoreType copy-constructed from another FirestoreType has internal_
//     set to a non-nullptr instance of FirestoreTypeInternal, which should be
//     equal to the internal_ of the other FirestoreType instance but we cannot
//     verify this yet.
//   * FirestoreType move-constructed from another FirestoreType has internal_
//     set to the internal_ of the other FirestoreType, which now should has
//     nullptr as its internal but we do not check that because the standard
//     forbids the programming practise to do anything on it after move.
// Here we assume FirestoreTypeInternal is default constructable.
template <typename FirestoreType, typename FirestoreTypeInternal>
void AssertWrapperConstructionContract() {
  FirestoreType default_instance;
  EXPECT_EQ(nullptr, FirestoreInternal::Internal<FirestoreTypeInternal>(
                         default_instance));

  FirestoreTypeInternal* internal = NewInternal<FirestoreTypeInternal>();
  FirestoreType instance = FirestoreInternal::Wrap(internal);
  EXPECT_EQ(internal,
            FirestoreInternal::Internal<FirestoreTypeInternal>(instance));

  FirestoreType instance_copy(instance);
  EXPECT_NE(nullptr,
            FirestoreInternal::Internal<FirestoreTypeInternal>(instance_copy));
  EXPECT_NE(internal,
            FirestoreInternal::Internal<FirestoreTypeInternal>(instance_copy));

  FirestoreType instance_move(std::move(instance));
  EXPECT_EQ(internal,
            FirestoreInternal::Internal<FirestoreTypeInternal>(instance_move));
}

// A helper that tests the common contract of the assignment logic of Firestore
// wrapper class.
//   * FirestoreType copy-assigned from another FirestoreType has internal_
//     set to a non-nullptr instance of FirestoreTypeInternal, which should be
//     equal to the internal_ of the other FirestoreType instance but we cannot
//     verify this yet.
//   * FirestoreType move-constructed from another FirestoreType has internal_
//     set to the internal_ of the other FirestoreType, which now should has
//     nullptr as its internal but we do not check that because the standard
//     forbids the programming practise to do anything on it after move.
// Here we assume irestoreTypeInternal is default constructable.
template <typename FirestoreType, typename FirestoreTypeInternal>
void AssertWrapperAssignmentContract() {
  FirestoreTypeInternal* internal = NewInternal<FirestoreTypeInternal>();
  FirestoreType instance = FirestoreInternal::Wrap(internal);
  FirestoreType instance_copy;
  instance_copy = instance;
  EXPECT_NE(nullptr,
            FirestoreInternal::Internal<FirestoreTypeInternal>(instance_copy));
  EXPECT_NE(internal,
            FirestoreInternal::Internal<FirestoreTypeInternal>(instance_copy));

  FirestoreType instance_move;
  instance_move = std::move(instance);
  EXPECT_EQ(internal,
            FirestoreInternal::Internal<FirestoreTypeInternal>(instance_move));
}

#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

}  // namespace testutil
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_WRAPPER_ASSERTIONS_H_
