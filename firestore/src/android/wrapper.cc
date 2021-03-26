#include "firestore/src/android/wrapper.h"

#include <iterator>

#include "app/src/assert.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Object;

}  // namespace

Wrapper::Wrapper(FirestoreInternal* firestore, const Object& obj)
    : firestore_(firestore), obj_(obj) {
  FIREBASE_ASSERT(obj);
}

Wrapper::Wrapper() {
  Firestore* firestore = Firestore::GetInstance();
  FIREBASE_ASSERT(firestore != nullptr);
  firestore_ = firestore->internal_;
  FIREBASE_ASSERT(firestore_ != nullptr);
}

Wrapper::Wrapper(Wrapper* rhs) : Wrapper() {
  if (rhs) {
    firestore_ = rhs->firestore_;
    FIREBASE_ASSERT(firestore_ != nullptr);
    obj_ = rhs->obj_;
  }
}

Wrapper::~Wrapper() = default;

jni::Env Wrapper::GetEnv() const { return firestore_->GetEnv(); }

Object Wrapper::ToJava(const FieldValue& value) {
  return FieldValueInternal::ToJava(value);
}

}  // namespace firestore
}  // namespace firebase
