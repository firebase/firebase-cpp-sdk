#include "firestore/src/android/wrapper.h"

#include <iterator>

#include "app/src/assert.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {

// clang-format off
#define OBJECT_METHOD(X) X(Equals, "equals", "(Ljava/lang/Object;)Z")
// clang-format on

METHOD_LOOKUP_DECLARATION(object, OBJECT_METHOD)
METHOD_LOOKUP_DEFINITION(object, "java/lang/Object", OBJECT_METHOD)

Wrapper::Wrapper(FirestoreInternal* firestore, jobject obj)
    : Wrapper(firestore, obj, AllowNullObject::Yes) {
  FIREBASE_ASSERT(obj != nullptr);
}

Wrapper::Wrapper(jclass clazz, jmethodID method_id, ...) {
  // Set firestore_.
  Firestore* firestore = Firestore::GetInstance();
  FIREBASE_ASSERT(firestore != nullptr);
  firestore_ = firestore->internal_;
  FIREBASE_ASSERT(firestore_ != nullptr);

  // Create a jobject.
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  va_list args;
  va_start(args, method_id);
  jobject obj = env->NewObjectV(clazz, method_id, args);
  va_end(args);
  CheckAndClearJniExceptions(env);

  // Set obj_,
  FIREBASE_ASSERT(obj != nullptr);
  obj_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);
}

Wrapper::Wrapper(const Wrapper& wrapper)
    : Wrapper(wrapper.firestore_, wrapper.obj_, AllowNullObject::Yes) {}

Wrapper::Wrapper(Wrapper&& wrapper) noexcept
    : firestore_(wrapper.firestore_), obj_(wrapper.obj_) {
  FIREBASE_ASSERT(firestore_ != nullptr);
  wrapper.obj_ = nullptr;
}

Wrapper::Wrapper() : obj_(nullptr) {
  Firestore* firestore = Firestore::GetInstance();
  FIREBASE_ASSERT(firestore != nullptr);
  firestore_ = firestore->internal_;
  FIREBASE_ASSERT(firestore_ != nullptr);
}

Wrapper::Wrapper(Wrapper* rhs) : Wrapper() {
  if (rhs) {
    firestore_ = rhs->firestore_;
    FIREBASE_ASSERT(firestore_ != nullptr);
    obj_ = firestore_->app()->GetJNIEnv()->NewGlobalRef(rhs->obj_);
  }
}

Wrapper::Wrapper(FirestoreInternal* firestore, jobject obj, AllowNullObject)
    : firestore_(firestore),
      // NewGlobalRef() is supposed to accept Null Java object and return Null,
      // which happens to be nullptr in C++.
      obj_(firestore->app()->GetJNIEnv()->NewGlobalRef(obj)) {}

Wrapper::~Wrapper() {
  if (obj_ != nullptr) {
    firestore_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

jni::Env Wrapper::GetEnv() const {
  // TODO(mcg): Investigate whether or not this method can be made static.
  // Whether or not this is possible depends on whether or not we need to flag
  // the current Firestore instance as somehow broken in response to an
  // exception.
  jni::Env env;
  env.SetUnhandledExceptionHandler(GlobalUnhandledExceptionHandler, nullptr);
  return env;
}

bool Wrapper::EqualsJavaObject(const Wrapper& other) const {
  if (obj_ == other.obj_) {
    return true;
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      obj_, object::GetMethodId(object::kEquals), other.obj_);
  CheckAndClearJniExceptions(env);
  return static_cast<bool>(result);
}

/* static */
jobject Wrapper::MapFieldValueToJavaMap(FirestoreInternal* firestore,
                                        const MapFieldValue& data) {
  JNIEnv* env = firestore->app()->GetJNIEnv();

  // Creates an empty Java HashMap (implementing Map) object.
  jobject result =
      env->NewObject(util::hash_map::GetClass(),
                     util::hash_map::GetMethodId(util::hash_map::kConstructor));
  CheckAndClearJniExceptions(env);

  // Adds each mapping.
  jmethodID put_method = util::map::GetMethodId(util::map::kPut);
  for (const auto& kv : data) {
    jobject key = env->NewStringUTF(kv.first.c_str());
    // Map::Put() returns previously associated value or null, which we have
    // no use of.
    env->CallObjectMethod(result, put_method, key,
                          kv.second.internal_->java_object());
    env->DeleteLocalRef(key);
    CheckAndClearJniExceptions(env);
  }

  return result;
}

/* static */
jobjectArray Wrapper::MapFieldPathValueToJavaArray(
    FirestoreInternal* firestore, MapFieldPathValue::const_iterator begin,
    MapFieldPathValue::const_iterator end) {
  JNIEnv* env = firestore->app()->GetJNIEnv();

  const auto size = std::distance(begin, end) * 2;
  jobjectArray result = env->NewObjectArray(size, util::object::GetClass(),
                                            /*initialElement=*/nullptr);
  CheckAndClearJniExceptions(env);

  int index = 0;
  for (auto iter = begin; iter != end; ++iter) {
    jobject field = FieldPathConverter::ToJavaObject(env, iter->first);
    env->SetObjectArrayElement(result, index, field);
    env->DeleteLocalRef(field);
    ++index;

    env->SetObjectArrayElement(result, index,
                               iter->second.internal_->java_object());
    ++index;
    CheckAndClearJniExceptions(env);
  }

  return result;
}

/* static */
bool Wrapper::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = object::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void Wrapper::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  object::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}
}  // namespace firestore
}  // namespace firebase
