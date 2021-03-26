#include "firestore/src/android/field_value_android.h"

#include "app/meta/move.h"
#include "firestore/src/android/blob_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/geo_point_android.h"
#include "firestore/src/android/timestamp_android.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/array_list.h"
#include "firestore/src/jni/boolean.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/double.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/iterator.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/long.h"
#include "firestore/src/jni/set.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Array;
using jni::ArrayList;
using jni::Boolean;
using jni::Class;
using jni::Double;
using jni::Env;
using jni::HashMap;
using jni::Iterator;
using jni::List;
using jni::Local;
using jni::Long;
using jni::Map;
using jni::Object;
using jni::StaticMethod;
using jni::String;

using Type = FieldValue::Type;

// com.google.firebase.firestore.FieldValue is the public type which contains
// static methods to build sentinel values.
constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/FieldValue";
StaticMethod<Object> kArrayRemove(
    "arrayRemove",
    "([Ljava/lang/Object;)Lcom/google/firebase/firestore/FieldValue;");
StaticMethod<Object> kArrayUnion(
    "arrayUnion",
    "([Ljava/lang/Object;)Lcom/google/firebase/firestore/FieldValue;");
StaticMethod<Object> kDelete("delete",
                             "()Lcom/google/firebase/firestore/FieldValue;");
StaticMethod<Object> kIncrementInteger(
    "increment", "(J)Lcom/google/firebase/firestore/FieldValue;");
StaticMethod<Object> kIncrementDouble(
    "increment", "(D)Lcom/google/firebase/firestore/FieldValue;");
StaticMethod<Object> kServerTimestamp(
    "serverTimestamp", "()Lcom/google/firebase/firestore/FieldValue;");

}  // namespace

void FieldValueInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kArrayRemove, kArrayUnion, kDelete,
                   kIncrementInteger, kIncrementDouble, kServerTimestamp);
}

FieldValue FieldValueInternal::Create(Env& env, const Object& object) {
  // Treat a null object as a null FieldValue.
  if (!env.ok()) return {};
  return FieldValue(new FieldValueInternal(Type::kNull, object));
}

FieldValue FieldValueInternal::Create(Env& env, Type type,
                                      const Object& object) {
  if (!env.ok() || !object) return {};
  return FieldValue(new FieldValueInternal(type, object));
}

FieldValueInternal::FieldValueInternal() : cached_type_(Type::kNull) {}

FieldValueInternal::FieldValueInternal(const Object& object)
    : object_(object), cached_type_(Type::kNull) {}

FieldValueInternal::FieldValueInternal(Type type, const Object& object)
    : object_(object), cached_type_(type) {}

FieldValueInternal::FieldValueInternal(bool value)
    : cached_type_(Type::kBoolean) {
  Env env = GetEnv();
  object_ = Boolean::Create(env, value);
}

FieldValueInternal::FieldValueInternal(int64_t value)
    : cached_type_(Type::kInteger) {
  Env env = GetEnv();
  object_ = Long::Create(env, value);
}

FieldValueInternal::FieldValueInternal(double value)
    : cached_type_(Type::kDouble) {
  Env env = GetEnv();
  object_ = Double::Create(env, value);
}

FieldValueInternal::FieldValueInternal(Timestamp value)
    : cached_type_(Type::kTimestamp) {
  Env env = GetEnv();
  object_ = TimestampInternal::Create(env, value);
}

FieldValueInternal::FieldValueInternal(std::string value)
    : cached_type_(Type::kString) {
  Env env = GetEnv();
  object_ = env.NewStringUtf(value);
}

// We do not initialize cached_blob_ with value here as the instance constructed
// with const uint8_t* is generally used for updating Firestore while
// cached_blob_ is only needed when reading from Firestore and calling with
// blob_value().
FieldValueInternal::FieldValueInternal(const uint8_t* value, size_t size)
    : cached_type_(Type::kBlob) {
  Env env = GetEnv();
  object_ = BlobInternal::Create(env, value, size);
}

FieldValueInternal::FieldValueInternal(DocumentReference value)
    : cached_type_{Type::kReference} {
  if (value.internal_ != nullptr) {
    object_ = value.internal_->ToJava();
  }
}

FieldValueInternal::FieldValueInternal(GeoPoint value)
    : cached_type_(Type::kGeoPoint) {
  Env env = GetEnv();
  object_ = GeoPointInternal::Create(env, value);
}

FieldValueInternal::FieldValueInternal(std::vector<FieldValue> value)
    : cached_type_(Type::kArray) {
  Env env = GetEnv();
  Local<ArrayList> list = ArrayList::Create(env, value.size());
  for (const FieldValue& element : value) {
    // TODO(b/150016438): don't conflate invalid `FieldValue`s and null.
    list.Add(env, ToJava(element));
  }
  object_ = list;
}

FieldValueInternal::FieldValueInternal(MapFieldValue value)
    : cached_type_(Type::kMap) {
  Env env = GetEnv();
  Local<HashMap> map = HashMap::Create(env);
  for (const auto& kv : value) {
    // TODO(b/150016438): don't conflate invalid `FieldValue`s and null.
    Local<String> key = env.NewStringUtf(kv.first);
    map.Put(env, key, ToJava(kv.second));
  }
  object_ = map;
}

Type FieldValueInternal::type() const {
  if (cached_type_ != Type::kNull) {
    return cached_type_;
  }
  if (!object_) {
    return Type::kNull;
  }

  // We do not have any knowledge on the type yet. Check the runtime type with
  // each known type.
  Env env = GetEnv();
  if (env.IsInstanceOf(object_, Boolean::GetClass())) {
    cached_type_ = Type::kBoolean;
    return Type::kBoolean;
  }
  if (env.IsInstanceOf(object_, Long::GetClass())) {
    cached_type_ = Type::kInteger;
    return Type::kInteger;
  }
  if (env.IsInstanceOf(object_, Double::GetClass())) {
    cached_type_ = Type::kDouble;
    return Type::kDouble;
  }
  if (env.IsInstanceOf(object_, TimestampInternal::GetClass())) {
    cached_type_ = Type::kTimestamp;
    return Type::kTimestamp;
  }
  if (env.IsInstanceOf(object_, String::GetClass())) {
    cached_type_ = Type::kString;
    return Type::kString;
  }
  if (env.IsInstanceOf(object_, BlobInternal::GetClass())) {
    cached_type_ = Type::kBlob;
    return Type::kBlob;
  }
  if (env.IsInstanceOf(object_, DocumentReferenceInternal::GetClass())) {
    cached_type_ = Type::kReference;
    return Type::kReference;
  }
  if (env.IsInstanceOf(object_, GeoPointInternal::GetClass())) {
    cached_type_ = Type::kGeoPoint;
    return Type::kGeoPoint;
  }
  if (env.IsInstanceOf(object_, List::GetClass())) {
    cached_type_ = Type::kArray;
    return Type::kArray;
  }
  if (env.IsInstanceOf(object_, Map::GetClass())) {
    cached_type_ = Type::kMap;
    return Type::kMap;
  }

  FIREBASE_ASSERT_MESSAGE(false, "Unsupported FieldValue type: %s",
                          Class::GetClassName(env, object_).c_str());
  return Type::kNull;
}

bool FieldValueInternal::boolean_value() const {
  Env env = GetEnv();
  return Cast<Boolean>(env, Type::kBoolean).BooleanValue(env);
}

int64_t FieldValueInternal::integer_value() const {
  Env env = GetEnv();
  return Cast<Long>(env, Type::kInteger).LongValue(env);
}

double FieldValueInternal::double_value() const {
  Env env = GetEnv();
  return Cast<Double>(env, Type::kDouble).DoubleValue(env);
}

Timestamp FieldValueInternal::timestamp_value() const {
  Env env = GetEnv();
  return Cast<TimestampInternal>(env, Type::kTimestamp).ToPublic(env);
}

std::string FieldValueInternal::string_value() const {
  Env env = GetEnv();
  return Cast<String>(env, Type::kString).ToString(env);
}

const uint8_t* FieldValueInternal::blob_value() const {
  static_assert(sizeof(uint8_t) == sizeof(jbyte),
                "uint8_t and jbyte must be of same size");

  Env env = GetEnv();
  EnsureCachedBlob(env);
  if (!env.ok() || cached_blob_.get() == nullptr) {
    return nullptr;
  }

  if (cached_blob_->empty()) {
    // The return value doesn't matter, but we can't return
    // `&cached_blob->front()` because calling `front` on an empty vector is
    // undefined behavior. When we drop support for STLPort, we can use `data`
    // instead which is defined, even for empty vectors.
    // TODO(b/163140650): remove this special case.
    return nullptr;
  }

  return &(cached_blob_->front());
}

size_t FieldValueInternal::blob_size() const {
  Env env = GetEnv();
  EnsureCachedBlob(env);
  if (!env.ok() || cached_blob_.get() == nullptr) {
    return 0;
  }

  return cached_blob_->size();
}

void FieldValueInternal::EnsureCachedBlob(Env& env) const {
  auto blob = Cast<BlobInternal>(env, Type::kBlob);
  if (cached_blob_.get() != nullptr) {
    return;
  }

  Local<Array<uint8_t>> bytes = blob.ToBytes(env);
  size_t size = bytes.Size(env);

  auto result = MakeShared<std::vector<uint8_t>>(size);
  env.GetArrayRegion(bytes, 0, size, &(result->front()));

  if (env.ok()) {
    cached_blob_ = Move(result);
  }
}

DocumentReference FieldValueInternal::reference_value() const {
  Env env = GetEnv();
  auto reference = Cast<Object>(env, Type::kReference);
  return DocumentReferenceInternal::Create(env, reference);
}

GeoPoint FieldValueInternal::geo_point_value() const {
  Env env = GetEnv();
  return Cast<GeoPointInternal>(env, Type::kGeoPoint).ToPublic(env);
}

std::vector<FieldValue> FieldValueInternal::array_value() const {
  Env env = GetEnv();
  auto list = Cast<List>(env, Type::kArray);
  size_t size = list.Size(env);

  std::vector<FieldValue> result;
  result.reserve(size);

  for (size_t i = 0; i < size; ++i) {
    Local<Object> element = list.Get(env, i);
    result.push_back(FieldValueInternal::Create(env, element));
  }

  if (!env.ok()) return std::vector<FieldValue>();
  return result;
}

MapFieldValue FieldValueInternal::map_value() const {
  Env env = GetEnv();
  auto map = Cast<Map>(env, Type::kMap);

  MapFieldValue result;
  Local<Iterator> iter = map.KeySet(env).Iterator(env);

  while (iter.HasNext(env)) {
    Local<Object> java_key = iter.Next(env);
    std::string key = java_key.ToString(env);

    Local<Object> java_value = map.Get(env, java_key);
    FieldValue value = FieldValueInternal::Create(env, java_value);

    result.insert(std::make_pair(firebase::Move(key), firebase::Move(value)));
  }

  if (!env.ok()) return MapFieldValue();
  return result;
}

FieldValue FieldValueInternal::Delete() {
  Env env = GetEnv();
  return Create(env, Type::kDelete, env.Call(kDelete));
}

FieldValue FieldValueInternal::ServerTimestamp() {
  Env env = GetEnv();
  return Create(env, Type::kServerTimestamp, env.Call(kServerTimestamp));
}

FieldValue FieldValueInternal::ArrayUnion(std::vector<FieldValue> elements) {
  Env env = GetEnv();
  auto array = MakeArray(env, elements);
  return Create(env, Type::kArrayUnion, env.Call(kArrayUnion, array));
}

FieldValue FieldValueInternal::ArrayRemove(std::vector<FieldValue> elements) {
  Env env = GetEnv();
  auto array = MakeArray(env, elements);
  return Create(env, Type::kArrayRemove, env.Call(kArrayRemove, array));
}

FieldValue FieldValueInternal::IntegerIncrement(int64_t by_value) {
  Env env = GetEnv();
  Local<Object> increment = env.Call(kIncrementInteger, by_value);
  return Create(env, Type::kIncrementInteger, increment);
}

FieldValue FieldValueInternal::DoubleIncrement(double by_value) {
  Env env = GetEnv();
  Local<Object> increment = env.Call(kIncrementDouble, by_value);
  return Create(env, Type::kIncrementDouble, increment);
}

bool operator==(const FieldValueInternal& lhs, const FieldValueInternal& rhs) {
  Env env = FieldValueInternal::GetEnv();
  return Object::Equals(env, lhs.object_, rhs.object_);
}

template <typename T>
T FieldValueInternal::Cast(jni::Env& env, Type type) const {
  if (cached_type_ == Type::kNull) {
    FIREBASE_ASSERT(env.IsInstanceOf(object_, T::GetClass()));
    cached_type_ = type;
  } else {
    FIREBASE_ASSERT(cached_type_ == type);
  }
  auto typed_value = static_cast<jni::JniType<T>>(object_.get());
  return T(typed_value);
}

Local<Array<Object>> FieldValueInternal::MakeArray(
    Env& env, const std::vector<FieldValue>& elements) {
  auto result = env.NewArray(elements.size(), Object::GetClass());
  for (int i = 0; i < elements.size(); ++i) {
    result.Set(env, i, ToJava(elements[i]));
  }
  return result;
}

Env FieldValueInternal::GetEnv() { return FirestoreInternal::GetEnv(); }

Object FieldValueInternal::ToJava(const FieldValue& value) {
  return value.internal_ ? value.internal_->object_ : Object();
}

}  // namespace firestore
}  // namespace firebase
