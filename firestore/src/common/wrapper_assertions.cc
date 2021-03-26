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
