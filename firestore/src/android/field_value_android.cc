#include "firestore/src/android/field_value_android.h"

#include <jni.h>

#include "app/meta/move.h"
#include "app/src/util_android.h"
#include "firestore/src/android/blob_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/geo_point_android.h"
#include "firestore/src/android/timestamp_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

using Type = FieldValue::Type;

// com.google.firebase.firestore.FieldValue is the public type which contains
// static methods to build sentinel values.
// clang-format off
#define FIELD_VALUE_METHODS(X)                                                 \
  X(Delete, "delete", "()Lcom/google/firebase/firestore/FieldValue;",          \
    util::kMethodTypeStatic),                                                  \
  X(ServerTimestamp, "serverTimestamp",                                        \
    "()Lcom/google/firebase/firestore/FieldValue;", util::kMethodTypeStatic),  \
  X(ArrayUnion, "arrayUnion",                                                  \
    "([Ljava/lang/Object;)Lcom/google/firebase/firestore/FieldValue;",         \
    util::kMethodTypeStatic),                                                  \
  X(ArrayRemove, "arrayRemove",                                                \
    "([Ljava/lang/Object;)Lcom/google/firebase/firestore/FieldValue;",         \
    util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(field_value, FIELD_VALUE_METHODS)
METHOD_LOOKUP_DEFINITION(field_value,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/FieldValue",
                         FIELD_VALUE_METHODS)

jobject FieldValueInternal::delete_ = nullptr;
jobject FieldValueInternal::server_timestamp_ = nullptr;

FieldValueInternal::FieldValueInternal(FirestoreInternal* firestore,
                                       jobject obj)
    : Wrapper(firestore, obj, AllowNullObject::Yes),
      cached_type_(Type::kNull) {}

FieldValueInternal::FieldValueInternal(const FieldValueInternal& wrapper)
    : Wrapper(wrapper),
      cached_type_(wrapper.cached_type_),
      cached_blob_(wrapper.cached_blob_) {}

FieldValueInternal::FieldValueInternal(FieldValueInternal&& wrapper)
    : Wrapper(firebase::Move(wrapper)),
      cached_type_(wrapper.cached_type_),
      cached_blob_(wrapper.cached_blob_) {}

FieldValueInternal::FieldValueInternal() : cached_type_(Type::kNull) {
  // The null Java object is actually represented by a nullptr jobject value.
  obj_ = nullptr;
}

FieldValueInternal::FieldValueInternal(bool value)
    : Wrapper(
          util::boolean_class::GetClass(),
          util::boolean_class::GetMethodId(util::boolean_class::kConstructor),
          static_cast<jboolean>(value)),
      cached_type_(Type::kBoolean) {}

FieldValueInternal::FieldValueInternal(int64_t value)
    : Wrapper(util::long_class::GetClass(),
              util::long_class::GetMethodId(util::long_class::kConstructor),
              static_cast<jlong>(value)),
      cached_type_(Type::kInteger) {}

FieldValueInternal::FieldValueInternal(double value)
    : Wrapper(util::double_class::GetClass(),
              util::double_class::GetMethodId(util::double_class::kConstructor),
              static_cast<jdouble>(value)),
      cached_type_(Type::kDouble) {}

FieldValueInternal::FieldValueInternal(Timestamp value)
    : cached_type_(Type::kTimestamp) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject obj = TimestampInternal::TimestampToJavaTimestamp(env, value);
  obj_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);
}

FieldValueInternal::FieldValueInternal(std::string value)
    : cached_type_(Type::kString) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject str = env->NewStringUTF(value.c_str());
  obj_ = env->NewGlobalRef(str);
  env->DeleteLocalRef(str);
  CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(obj_ != nullptr);
}

// We do not initialize cached_blob_ with value here as the instance constructed
// with const uint8_t* is generally used for updating Firestore while
// cached_blob_ is only needed when reading from Firestore and calling with
// blob_value().
FieldValueInternal::FieldValueInternal(const uint8_t* value, size_t size)
    : cached_type_(Type::kBlob) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject obj = BlobInternal::BlobToJavaBlob(env, value, size);
  obj_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);
  CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(obj_ != nullptr);
}

FieldValueInternal::FieldValueInternal(DocumentReference value)
    : Wrapper(value.internal_), cached_type_{Type::kReference} {}

FieldValueInternal::FieldValueInternal(GeoPoint value)
    : cached_type_(Type::kGeoPoint) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jobject obj = GeoPointInternal::GeoPointToJavaGeoPoint(env, value);
  obj_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);
}

FieldValueInternal::FieldValueInternal(std::vector<FieldValue> value)
    : Wrapper(
          util::array_list::GetClass(),
          util::array_list::GetMethodId(util::array_list::kConstructorWithSize),
          static_cast<jint>(value.size())),
      cached_type_(Type::kArray) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jmethodID add_method = util::array_list::GetMethodId(util::array_list::kAdd);
  for (const FieldValue& element : value) {
    // ArrayList.Add() always returns true, which we have no use for.
    // TODO(b/150016438): don't conflate invalid `FieldValue`s and null.
    env->CallBooleanMethod(obj_, add_method, TryGetJobject(element));
  }
  CheckAndClearJniExceptions(env);
}

FieldValueInternal::FieldValueInternal(MapFieldValue value)
    : Wrapper(util::hash_map::GetClass(),
              util::hash_map::GetMethodId(util::hash_map::kConstructor)),
      cached_type_(Type::kMap) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jmethodID put_method = util::map::GetMethodId(util::map::kPut);
  for (const auto& kv : value) {
    jobject key = env->NewStringUTF(kv.first.c_str());
    // Map::Put() returns previously associated value or null, which we have no
    // use for.
    // TODO(b/150016438): don't conflate invalid `FieldValue`s and null.
    env->CallObjectMethod(obj_, put_method, key, TryGetJobject(kv.second));
    env->DeleteLocalRef(key);
  }
  CheckAndClearJniExceptions(env);
}

Type FieldValueInternal::type() const {
  if (cached_type_ != Type::kNull) {
    return cached_type_;
  }
  if (obj_ == nullptr) {
    return Type::kNull;
  }

  // We do not have any knowledge on the type yet. Check the runtime type with
  // each known type.
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  if (env->IsInstanceOf(obj_, util::boolean_class::GetClass())) {
    cached_type_ = Type::kBoolean;
    return Type::kBoolean;
  }
  if (env->IsInstanceOf(obj_, util::long_class::GetClass())) {
    cached_type_ = Type::kInteger;
    return Type::kInteger;
  }
  if (env->IsInstanceOf(obj_, util::double_class::GetClass())) {
    cached_type_ = Type::kDouble;
    return Type::kDouble;
  }
  if (env->IsInstanceOf(obj_, TimestampInternal::GetClass())) {
    cached_type_ = Type::kTimestamp;
    return Type::kTimestamp;
  }
  if (env->IsInstanceOf(obj_, util::string::GetClass())) {
    cached_type_ = Type::kString;
    return Type::kString;
  }
  if (env->IsInstanceOf(obj_, BlobInternal::GetClass())) {
    cached_type_ = Type::kBlob;
    return Type::kBlob;
  }
  if (env->IsInstanceOf(obj_, DocumentReferenceInternal::GetClass())) {
    cached_type_ = Type::kReference;
    return Type::kReference;
  }
  if (env->IsInstanceOf(obj_, GeoPointInternal::GetClass())) {
    cached_type_ = Type::kGeoPoint;
    return Type::kGeoPoint;
  }
  if (env->IsInstanceOf(obj_, util::list::GetClass())) {
    cached_type_ = Type::kArray;
    return Type::kArray;
  }
  if (env->IsInstanceOf(obj_, util::map::GetClass())) {
    cached_type_ = Type::kMap;
    return Type::kMap;
  }

  FIREBASE_ASSERT_MESSAGE(false, "Unsupported FieldValue type: %s",
                          util::JObjectClassName(env, obj_).c_str());
  return Type::kNull;
}

bool FieldValueInternal::boolean_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, util::boolean_class::GetClass()));
    cached_type_ = Type::kBoolean;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kBoolean);
  }

  return util::JBooleanToBool(env, obj_);
}

int64_t FieldValueInternal::integer_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, util::long_class::GetClass()));
    cached_type_ = Type::kInteger;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kInteger);
  }

  return util::JLongToInt64(env, obj_);
}

double FieldValueInternal::double_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, util::double_class::GetClass()));
    cached_type_ = Type::kDouble;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kDouble);
  }

  return util::JDoubleToDouble(env, obj_);
}

Timestamp FieldValueInternal::timestamp_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, TimestampInternal::GetClass()));
    cached_type_ = Type::kTimestamp;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kTimestamp);
  }

  return TimestampInternal::JavaTimestampToTimestamp(env, obj_);
}

std::string FieldValueInternal::string_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, util::string::GetClass()));
    cached_type_ = Type::kString;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kString);
  }

  return util::JStringToString(env, obj_);
}

const uint8_t* FieldValueInternal::blob_value() const {
  if (blob_size() == 0) {
    // Doesn't matter what we return. Not return &(cached_blob_.get()->front())
    // to avoid going into undefined-behavior world. Once we drop the support of
    // STLPort, we might just combine this case into the logic below by calling
    // cached_blob_.get()->data().
    return nullptr;
  }

  if (cached_blob_.get()) {
    return &(cached_blob_.get()->front());
  }

  size_t size = blob_size();
  // firebase::SharedPtr does not have set() API.
  cached_blob_ =
      SharedPtr<std::vector<uint8_t>>{new std::vector<uint8_t>(size)};

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jbyteArray bytes = BlobInternal::JavaBlobToJbyteArray(env, obj_);
  static_assert(sizeof(uint8_t) == sizeof(jbyte),
                "uint8_t and jbyte must be of same size");
  env->GetByteArrayRegion(
      bytes, 0, size, reinterpret_cast<jbyte*>(&(cached_blob_.get()->front())));
  env->DeleteLocalRef(bytes);

  CheckAndClearJniExceptions(env);
  return &(cached_blob_.get()->front());
}

size_t FieldValueInternal::blob_size() const {
  if (cached_blob_.get()) {
    return cached_blob_.get()->size();
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, BlobInternal::GetClass()));
    cached_type_ = Type::kBlob;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kBlob);
  }

  jbyteArray bytes = BlobInternal::JavaBlobToJbyteArray(env, obj_);
  jsize result = env->GetArrayLength(bytes);
  env->DeleteLocalRef(bytes);
  CheckAndClearJniExceptions(env);
  return static_cast<size_t>(result);
}

DocumentReference FieldValueInternal::reference_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(
        env->IsInstanceOf(obj_, DocumentReferenceInternal::GetClass()));
    cached_type_ = Type::kReference;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kReference);
  }

  if (!obj_) {
    // Reference may be invalid.
    return DocumentReference{};
  }
  return DocumentReference{new DocumentReferenceInternal(firestore_, obj_)};
}

GeoPoint FieldValueInternal::geo_point_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, GeoPointInternal::GetClass()));
    cached_type_ = Type::kGeoPoint;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kGeoPoint);
  }

  return GeoPointInternal::JavaGeoPointToGeoPoint(env, obj_);
}

std::vector<FieldValue> FieldValueInternal::array_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, util::list::GetClass()));
    cached_type_ = Type::kArray;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kArray);
  }

  std::vector<FieldValue> result;
  jint size =
      env->CallIntMethod(obj_, util::list::GetMethodId(util::list::kSize));
  result.reserve(size);
  CheckAndClearJniExceptions(env);
  for (jint i = 0; i < size; ++i) {
    jobject element = env->CallObjectMethod(
        obj_, util::list::GetMethodId(util::list::kGet), i);
    // Cannot call emplace_back as the constructor is protected.
    result.push_back(FieldValue(new FieldValueInternal(firestore_, element)));
    env->DeleteLocalRef(element);
    CheckAndClearJniExceptions(env);
  }
  return result;
}

MapFieldValue FieldValueInternal::map_value() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  // Make sure this instance is of correct type.
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env->IsInstanceOf(obj_, util::map::GetClass()));
    cached_type_ = Type::kMap;
  } else {
    FIREBASE_ASSERT(cached_type_ == Type::kMap);
  }

  // The following logic is copied from firebase::util::JavaMapToStdMapTemplate,
  // which we cannot use here directly as it presumes that key and value have
  // the same type.
  MapFieldValue result;
  // Set<Object> key_set = obj_.keySet();
  jobject key_set =
      env->CallObjectMethod(obj_, util::map::GetMethodId(util::map::kKeySet));
  CheckAndClearJniExceptions(env);
  // Iterator iter = key_set.iterator();
  jobject iter = env->CallObjectMethod(
      key_set, util::set::GetMethodId(util::set::kIterator));
  CheckAndClearJniExceptions(env);

  // while (iter.hasNext())
  while (env->CallBooleanMethod(
      iter, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    CheckAndClearJniExceptions(env);

    // T key = iter.next();
    // T value = obj_.get(key);
    jobject key_object = env->CallObjectMethod(
        iter, util::iterator::GetMethodId(util::iterator::kNext));
    CheckAndClearJniExceptions(env);
    std::string key = util::JStringToString(env, key_object);

    jobject value_object = env->CallObjectMethod(
        obj_, util::map::GetMethodId(util::map::kGet), key_object);
    CheckAndClearJniExceptions(env);
    FieldValue value =
        FieldValue(new FieldValueInternal(firestore_, value_object));

    env->DeleteLocalRef(key_object);
    env->DeleteLocalRef(value_object);

    // Once deprecated STLPort support, change back to call emplace.
    result.insert(std::make_pair(firebase::Move(key), firebase::Move(value)));
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);

  return result;
}

double FieldValueInternal::double_increment_value() const {
  // TODO(varconst): implement and test.
  return {};
}

int64_t FieldValueInternal::integer_increment_value() const {
  // TODO(varconst): implement and test.
  return {};
}

/* static */
FieldValue FieldValueInternal::Delete() {
  FieldValueInternal* value = new FieldValueInternal();
  value->cached_type_ = Type::kDelete;

  JNIEnv* env = value->firestore_->app()->GetJNIEnv();
  value->obj_ = env->NewGlobalRef(delete_);

  return FieldValue{value};
}

/* static */
FieldValue FieldValueInternal::ServerTimestamp() {
  FieldValueInternal* value = new FieldValueInternal();
  value->cached_type_ = Type::kServerTimestamp;

  JNIEnv* env = value->firestore_->app()->GetJNIEnv();
  value->obj_ = env->NewGlobalRef(server_timestamp_);

  return FieldValue{value};
}

/* static */
FieldValue FieldValueInternal::ArrayUnion(std::vector<FieldValue> elements) {
  FieldValueInternal* value = new FieldValueInternal();
  value->cached_type_ = Type::kArrayUnion;

  JNIEnv* env = value->firestore_->app()->GetJNIEnv();
  jobjectArray array =
      env->NewObjectArray(elements.size(), util::object::GetClass(), nullptr);
  for (int i = 0; i < elements.size(); ++i) {
    env->SetObjectArrayElement(array, i, elements[i].internal_->obj_);
  }

  jobject obj = env->CallStaticObjectMethod(
      field_value::GetClass(),
      field_value::GetMethodId(field_value::kArrayUnion), array);
  env->DeleteLocalRef(array);
  CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(obj != nullptr);
  value->obj_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);

  return FieldValue{value};
}

/* static */
FieldValue FieldValueInternal::ArrayRemove(std::vector<FieldValue> elements) {
  FieldValueInternal* value = new FieldValueInternal();
  value->cached_type_ = Type::kArrayRemove;
  JNIEnv* env = value->firestore_->app()->GetJNIEnv();
  jobjectArray array =
      env->NewObjectArray(elements.size(), util::object::GetClass(), nullptr);
  for (int i = 0; i < elements.size(); ++i) {
    env->SetObjectArrayElement(array, i, elements[i].internal_->obj_);
  }

  jobject obj = env->CallStaticObjectMethod(
      field_value::GetClass(),
      field_value::GetMethodId(field_value::kArrayRemove), array);
  env->DeleteLocalRef(array);
  CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(obj != nullptr);
  value->obj_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);
  return FieldValue{value};
}

/* static */
FieldValue FieldValueInternal::Increment(double) {
  // TODO(varconst): implement and test.
  return {};
}

/* static */
FieldValue FieldValueInternal::Increment(int64_t) {
  // TODO(varconst): implement and test.
  return {};
}

/* static */
bool FieldValueInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = field_value::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);

  // Cache singleton Java objects.
  jobject obj = env->CallStaticObjectMethod(
      field_value::GetClass(), field_value::GetMethodId(field_value::kDelete));
  FIREBASE_ASSERT(obj != nullptr);
  delete_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);

  obj = env->CallStaticObjectMethod(
      field_value::GetClass(),
      field_value::GetMethodId(field_value::kServerTimestamp));
  FIREBASE_ASSERT(obj != nullptr);
  server_timestamp_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);
  util::CheckAndClearJniExceptions(env);

  return result;
}

/* static */
void FieldValueInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  field_value::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);

  // Uncache singleton Java values.
  env->DeleteGlobalRef(delete_);
  delete_ = nullptr;
  env->DeleteGlobalRef(server_timestamp_);
  server_timestamp_ = nullptr;
}

bool operator==(const FieldValueInternal& lhs, const FieldValueInternal& rhs) {
  // Most likely only happens when comparing one with itself or both are Null.
  if (lhs.obj_ == rhs.obj_) {
    return true;
  }

  // If only one of them is Null, then they cannot equal.
  if (lhs.obj_ == nullptr || rhs.obj_ == nullptr) {
    return false;
  }

  return lhs.EqualsJavaObject(rhs);
}

jobject FieldValueInternal::TryGetJobject(const FieldValue& value) {
  return value.internal_ ? value.internal_->obj_ : nullptr;
}

}  // namespace firestore
}  // namespace firebase
