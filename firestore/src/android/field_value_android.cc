// Copyright 2020 Google LLC

#include "firestore/src/android/field_value_android.h"

#include <cmath>
#include <cstdint>
#include <utility>

#include "firestore/src/android/blob_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/geo_point_android.h"
#include "firestore/src/android/timestamp_android.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/array_list.h"
#include "firestore/src/jni/boolean.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/compare.h"
#include "firestore/src/jni/double.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/iterator.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/long.h"
#include "firestore/src/jni/set.h"

namespace firebase::firestore {
namespace {

using jni::ArenaRef;
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
  if (!env.ok()) return {};
  return FieldValue(object ? new FieldValueInternal(object)
                           : new FieldValueInternal());
}

FieldValue FieldValueInternal::Create(Env& env, Type type,
                                      const Object& object) {
  return (env.ok() && object) ? FieldValue(new FieldValueInternal(type, object))
                              : FieldValue{};
}

FieldValueInternal::FieldValueInternal() : cached_type_(Type::kNull) {}

FieldValueInternal::FieldValueInternal(const FieldValueInternal& other) {
  std::lock_guard<std::mutex> lock(other.mutex_);
  object_ = other.object_;
  cached_type_ = other.cached_type_;
  cached_blob_ = other.cached_blob_;
}

FieldValueInternal::FieldValueInternal(FieldValueInternal&& other) noexcept {
  std::lock_guard<std::mutex> lock(other.mutex_);
  object_ = std::move(other.object_);
  cached_type_ = other.cached_type_;
  cached_blob_ = std::move(other.cached_blob_);
}

FieldValueInternal& FieldValueInternal::operator=(
    const FieldValueInternal& other) {
  if (this != &other) {
    std::scoped_lock lock(mutex_, other.mutex_);
    object_ = other.object_;
    cached_type_ = other.cached_type_;
    cached_blob_ = other.cached_blob_;
  }
  return *this;
}

FieldValueInternal& FieldValueInternal::operator=(
    FieldValueInternal&& other) noexcept {
  if (this != &other) {
    std::scoped_lock lock(mutex_, other.mutex_);
    object_ = std::move(other.object_);
    cached_type_ = other.cached_type_;
    cached_blob_ = std::move(other.cached_blob_);
  }
  return *this;
}

FieldValueInternal::FieldValueInternal(const Object& object)
    : cached_type_(object ? std::nullopt : std::make_optional(Type::kNull)) {
  Env env = GetEnv();
  object_.reset(env, object);
}

FieldValueInternal::FieldValueInternal(Type type, const Object& object)
    : cached_type_(type) {
  Env env = GetEnv();
  object_.reset(env, object);
}

FieldValueInternal::FieldValueInternal(bool value)
    : cached_type_(Type::kBoolean) {
  Env env = GetEnv();
  object_.reset(env, Boolean::Create(env, value));
}

FieldValueInternal::FieldValueInternal(int64_t value)
    : cached_type_(Type::kInteger) {
  Env env = GetEnv();
  object_.reset(env, Long::Create(env, value));
}

FieldValueInternal::FieldValueInternal(double value)
    : cached_type_(Type::kDouble) {
  Env env = GetEnv();
  object_.reset(env, Double::Create(env, value));
}

FieldValueInternal::FieldValueInternal(Timestamp value)
    : cached_type_(Type::kTimestamp) {
  Env env = GetEnv();
  object_.reset(env, TimestampInternal::Create(env, value));
}

FieldValueInternal::FieldValueInternal(std::string value)
    : cached_type_(Type::kString) {
  Env env = GetEnv();
  object_.reset(env, env.NewStringUtf(value));
}

FieldValueInternal::FieldValueInternal(const uint8_t* value, size_t size)
    : cached_type_(Type::kBlob) {
  Env env = GetEnv();
  object_.reset(env, BlobInternal::Create(env, value, size));
}

FieldValueInternal::FieldValueInternal(DocumentReference value)
    : cached_type_{Type::kReference} {
  if (value.is_valid() && value.internal_) {
    Env env = GetEnv();
    object_.reset(env, value.internal_->ToJava());
  }
}

FieldValueInternal::FieldValueInternal(GeoPoint value)
    : cached_type_(Type::kGeoPoint) {
  Env env = GetEnv();
  object_.reset(env, GeoPointInternal::Create(env, value));
}

FieldValueInternal::FieldValueInternal(const std::vector<FieldValue>& value)
    : cached_type_(Type::kArray) {
  Env env = GetEnv();
  Local<ArrayList> list = ArrayList::Create(env, value.size());
  for (const FieldValue& element : value) list.Add(env, ToJava(element));
  object_.reset(env, list);
}

FieldValueInternal::FieldValueInternal(const MapFieldValue& value)
    : cached_type_(Type::kMap) {
  Env env = GetEnv();
  Local<HashMap> map = HashMap::Create(env);
  for (const auto& [k, v] : value) {
    map.Put(env, env.NewStringUtf(k), ToJava(v));
  }
  object_.reset(env, map);
}

Type FieldValueInternal::type() const {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cached_type_.has_value()) return *cached_type_;
  }
  Env env = GetEnv();
  Local<Object> object;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    object = object_.get(env);
  }
  Type resolved_type = Type::kNull;
  if (!object) {
    resolved_type = Type::kNull;
  } else if (env.IsInstanceOf(object, Boolean::GetClass())) {
    resolved_type = Type::kBoolean;
  } else if (env.IsInstanceOf(object, Long::GetClass())) {
    resolved_type = Type::kInteger;
  } else if (env.IsInstanceOf(object, Double::GetClass())) {
    resolved_type = Type::kDouble;
  } else if (env.IsInstanceOf(object, TimestampInternal::GetClass())) {
    resolved_type = Type::kTimestamp;
  } else if (env.IsInstanceOf(object, String::GetClass())) {
    resolved_type = Type::kString;
  } else if (env.IsInstanceOf(object, BlobInternal::GetClass())) {
    resolved_type = Type::kBlob;
  } else if (env.IsInstanceOf(object, DocumentReferenceInternal::GetClass())) {
    resolved_type = Type::kReference;
  } else if (env.IsInstanceOf(object, GeoPointInternal::GetClass())) {
    resolved_type = Type::kGeoPoint;
  } else if (env.IsInstanceOf(object, List::GetClass())) {
    resolved_type = Type::kArray;
  } else if (env.IsInstanceOf(object, Map::GetClass())) {
    resolved_type = Type::kMap;
  } else {
    FIREBASE_ASSERT_MESSAGE(false, "Unsupported FieldValue type: %s",
                            Class::GetClassName(env, object).c_str());
    return Type::kNull;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  cached_type_ = resolved_type;
  return resolved_type;
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
  Env env = GetEnv();
  EnsureCachedBlob(env);
  std::lock_guard<std::mutex> lock(mutex_);
  return (env.ok() && cached_blob_) ? cached_blob_->data() : nullptr;
}

size_t FieldValueInternal::blob_size() const {
  Env env = GetEnv();
  EnsureCachedBlob(env);
  std::lock_guard<std::mutex> lock(mutex_);
  return (env.ok() && cached_blob_) ? cached_blob_->size() : 0;
}

void FieldValueInternal::EnsureCachedBlob(Env& env) const {
  auto blob = Cast<BlobInternal>(env, Type::kBlob);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cached_blob_) return;
  }
  Local<Array<uint8_t>> bytes = blob.ToBytes(env);
  size_t size = bytes.Size(env);
  auto result = std::make_shared<std::vector<uint8_t>>(size);
  if (size > 0) env.GetArrayRegion(bytes, 0, size, result->data());

  std::lock_guard<std::mutex> lock(mutex_);
  if (!cached_blob_ && env.ok()) cached_blob_ = std::move(result);
}

DocumentReference FieldValueInternal::reference_value() const {
  Env env = GetEnv();
  auto reference = Cast<Object>(env, Type::kReference);
  return (env.ok() && reference)
             ? DocumentReferenceInternal::Create(env, reference)
             : DocumentReference{};
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
  for (size_t i = 0; i < size && env.ok(); ++i) {
    result.push_back(FieldValueInternal::Create(env, list.Get(env, i)));
  }
  return env.ok() ? result : std::vector<FieldValue>();
}

MapFieldValue FieldValueInternal::map_value() const {
  Env env = GetEnv();
  auto map = Cast<Map>(env, Type::kMap);
  MapFieldValue result;
  Local<Iterator> iter = map.KeySet(env).Iterator(env);
  while (iter.HasNext(env) && env.ok()) {
    Local<Object> java_key = iter.Next(env);
    result.emplace(java_key.ToString(env),
                   FieldValueInternal::Create(env, map.Get(env, java_key)));
  }
  return env.ok() ? result : MapFieldValue();
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
  return Create(env, Type::kArrayUnion,
                env.Call(kArrayUnion, MakeArray(env, elements)));
}

FieldValue FieldValueInternal::ArrayRemove(std::vector<FieldValue> elements) {
  Env env = GetEnv();
  return Create(env, Type::kArrayRemove,
                env.Call(kArrayRemove, MakeArray(env, elements)));
}

FieldValue FieldValueInternal::IntegerIncrement(int64_t by_value) {
  Env env = GetEnv();
  return Create(env, Type::kIncrementInteger,
                env.Call(kIncrementInteger, by_value));
}

FieldValue FieldValueInternal::DoubleIncrement(double by_value) {
  Env env = GetEnv();
  return Create(env, Type::kIncrementDouble,
                env.Call(kIncrementDouble, by_value));
}

bool operator==(const FieldValueInternal& lhs, const FieldValueInternal& rhs) {
  return lhs.type() == rhs.type() && jni::EqualityCompareJni(lhs, rhs);
}

template <typename T>
Local<T> FieldValueInternal::Cast(jni::Env& env, Type type) const {
  std::lock_guard<std::mutex> lock(mutex_);
  Local<Object> obj = object_.get(env);
  if (!cached_type_.has_value()) {
    FIREBASE_ASSERT(env.IsInstanceOf(obj, T::GetClass()));
    cached_type_ = type;
  } else {
    FIREBASE_ASSERT(*cached_type_ == type);
  }
  return std::move(obj).CastTo<T>();
}

Local<Array<Object>> FieldValueInternal::MakeArray(
    Env& env, const std::vector<FieldValue>& elements) {
  auto result = env.NewArray(elements.size(), Object::GetClass());
  for (size_t i = 0; i < elements.size(); ++i) {
    result.Set(env, i, ToJava(elements[i]));
  }
  return result;
}

Env FieldValueInternal::GetEnv() { return FirestoreInternal::GetEnv(); }

Local<Object> FieldValueInternal::ToJava() const {
  Env env = GetEnv();
  std::lock_guard<std::mutex> lock(mutex_);
  return object_.get(env);
}

Local<Object> FieldValueInternal::ToJava(const FieldValue& value) {
  if (!value.is_valid() || !value.internal_) {
    SimpleThrowInvalidArgument("Invalid FieldValue provided.");
    return {};
  }
  Env env = GetEnv();
  return value.internal_->object_.get(env);
}

}  // namespace firebase::firestore
