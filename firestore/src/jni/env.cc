// Copyright 2020 Google LLC

#include "firestore/src/jni/env.h"

#include <cstdint>
#include <cstring>
#include <exception>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/string.h"
#include "firestore/src/jni/throwable.h"

#ifndef JNI_ENOMEM
#define JNI_ENOMEM -4
#define JNI_EEXIST -5
#define JNI_EINVAL -6
#endif

namespace firebase::firestore::jni {
namespace {

int CurrentExceptionCount() {
#if __cpp_exceptions
  return std::uncaught_exceptions();
#else
  return 0;
#endif
}

Method<bool> kObjectEquals("equals", "(Ljava/lang/Object;)Z");
Method<String> kObjectToString("toString", "()Ljava/lang/String;");
jclass g_object_class = nullptr;

Method<String> kClassGetName("getName", "()Ljava/lang/String;");
Method<bool> kClassIsArray("isArray", "()Z");

Constructor<String> kStringNewFromBytes("([BLjava/lang/String;)V");
Method<Array<uint8_t>> kStringGetBytes("getBytes", "(Ljava/lang/String;)[B");
jclass g_string_class = nullptr;
jstring g_utf8_string = nullptr;

}  // namespace

void Object::Initialize(Loader& loader) {
  if (jclass clazz = firebase::util::object::GetClass()) {
    g_object_class = clazz;
    loader.LoadFromExistingClass("java/lang/Object", clazz, kObjectEquals,
                                 kObjectToString);
  } else {
    g_object_class =
        loader.LoadClass("java/lang/Object", kObjectEquals, kObjectToString);
  }
}

Class Object::GetClass() { return Class(g_object_class); }

std::string Object::ToString(Env& env) const {
  if (!*this || !env.ok()) return "";
  Local<String> s = env.Call(*this, kObjectToString);
  return (env.ok() && s) ? s.ToString(env) : "";
}

bool Object::Equals(Env& env, const Object& other) const {
  if (!env.ok()) return false;
  if (!*this && !other) return true;
  return (*this && other) ? env.Call(*this, kObjectEquals, other) : false;
}

bool Object::Equals(Env& env, const Object& lhs, const Object& rhs) {
  if (!env.ok()) return false;
  if (lhs.get() == rhs.get()) return true;
  return (lhs && rhs) ? lhs.Equals(env, rhs) : false;
}

void Class::Initialize(Loader& loader) {
  loader.LoadClass("java/lang/Class", kClassGetName, kClassIsArray);
}

std::string Class::GetName(Env& env) const {
  if (!*this || !env.ok()) return "";
  Local<String> name = env.Call(*this, kClassGetName);
  return (env.ok() && name) ? name.ToString(env) : "";
}

std::string Class::GetClassName(Env& env, const Object& object) {
  if (!object || !env.ok()) return "";
  Local<Class> clazz = env.GetObjectClass(object);
  return (env.ok() && clazz) ? clazz.GetName(env) : "";
}

bool Class::IsArray(Env& env) const {
  return (*this && env.ok()) ? env.Call(*this, kClassIsArray) : false;
}

void String::Initialize(Env& env, Loader& loader) {
  if (!loader.ok() || !env.ok() || !env.get()) return;
  g_string_class = firebase::util::string::GetClass();
  loader.LoadFromExistingClass("java/lang/String", g_string_class,
                               kStringNewFromBytes, kStringGetBytes);
  if (!loader.ok() || !env.ok()) return;
  Terminate(env);
  jstring raw_utf8 = env.get()->NewStringUTF("UTF-8");
  if (!env.ok() || !raw_utf8) return;
  g_utf8_string = Global<String>(Local<String>(env.get(), raw_utf8)).release();
}

void String::Terminate(Env& env) {
  if (g_utf8_string && env.get()) {
    env.get()->DeleteGlobalRef(g_utf8_string);
    g_utf8_string = nullptr;
  }
}

Class String::GetClass() { return Class(g_string_class); }
String String::GetUtf8() { return String(g_utf8_string); }

Local<String> String::Create(Env& env, const Array<uint8_t>& bytes,
                             const String& encoding) {
  return env.New(kStringNewFromBytes, bytes, encoding);
}

Local<Array<uint8_t>> String::GetBytes(Env& env,
                                       const String& encoding) const {
  return env.Call(*this, kStringGetBytes, encoding);
}

std::string String::ToString(Env& env) const { return env.ToStringUtf(*this); }

std::string Throwable::GetMessage(Env& env) const {
  if (!*this || !env.get()) return "";
  ExceptionClearGuard block(env);
  return firebase::util::GetMessageFromException(env.get(), object_);
}

Env::Env() : Env(GetEnv()) {}

Env::Env(JNIEnv* env)
    : env_(env), initial_pending_exceptions_(CurrentExceptionCount()) {}

Env::~Env() noexcept(false) {
  if (exception_handler_ && env_ && !ok() &&
      CurrentExceptionCount() == initial_pending_exceptions_) {
    exception_handler_(*this, ExceptionOccurred(), context_);
  }
}

Local<Class> Env::FindClass(const char* name) {
  return (ok() && name) ? Local<Class>(env_, env_->FindClass(name))
                        : Local<Class>();
}

void Env::Throw(const Throwable& throwable) {
  if (!ok() || !throwable) return;
  jint result = env_->Throw(throwable.get());
  FIREBASE_ASSERT_MESSAGE(
      result == JNI_OK, "Failed to throw an exception %s: %s",
      ErrorDescription(throwable).c_str(), ErrorName(result));
}

void Env::ThrowNew(const Class& clazz, const char* message) {
  if (!ok() || !clazz) return;
  jint result = env_->ThrowNew(clazz.get(), message);
  FIREBASE_ASSERT_MESSAGE(
      result == JNI_OK, "Failed to throw %s with message %s: %s",
      ErrorDescription(clazz).c_str(), message ? message : "(null)",
      ErrorName(result));
}

Local<Throwable> Env::ExceptionOccurred() {
  return env_ ? Local<Throwable>(env_, env_->ExceptionOccurred())
              : Local<Throwable>();
}

void Env::ExceptionClear() {
  if (env_) env_->ExceptionClear();
}

Local<Throwable> Env::ClearExceptionOccurred() {
  if (!env_) return {};
  Local<Throwable> result = ExceptionOccurred();
  ExceptionClear();
  return result;
}

Local<Class> Env::GetObjectClass(const Object& object) {
  return (ok() && object) ? Local<Class>(env_, env_->GetObjectClass(object.get()))
                          : Local<Class>();
}

bool Env::IsInstanceOf(const Object& object, const Class& clazz) {
  return (ok() && object && clazz) &&
         env_->IsInstanceOf(object.get(), clazz.get()) != JNI_FALSE;
}

bool Env::IsSameObject(const Object& object1, const Object& object2) {
  return ok() && env_->IsSameObject(object1.get(), object2.get()) != JNI_FALSE;
}

jmethodID Env::GetMethodId(const Class& clazz, const char* name,
                           const char* sig) {
  return (ok() && clazz && name && sig)
             ? env_->GetMethodID(clazz.get(), name, sig)
             : nullptr;
}

jfieldID Env::GetStaticFieldId(const Class& clazz, const char* name,
                               const char* sig) {
  return (ok() && clazz && name && sig)
             ? env_->GetStaticFieldID(ToJni(clazz), name, sig)
             : nullptr;
}

jmethodID Env::GetStaticMethodId(const Class& clazz, const char* name,
                                 const char* sig) {
  return (ok() && clazz && name && sig)
             ? env_->GetStaticMethodID(ToJni(clazz), name, sig)
             : nullptr;
}

Local<String> Env::NewStringUtf(const char* bytes) {
  return bytes ? NewStringUtf(bytes, std::strlen(bytes)) : Local<String>();
}

Local<String> Env::NewStringUtf(const char* bytes, size_t size) {
  if (!ok() || (!bytes && size > 0)) return {};
  Local<Array<uint8_t>> java_bytes = NewArray<uint8_t>(size);
  if (!ok() || !java_bytes) return {};
  SetArrayRegion(java_bytes, 0, size, reinterpret_cast<const uint8_t*>(bytes));
  return ok() ? String::Create(*this, java_bytes, String::GetUtf8())
              : Local<String>();
}

std::string Env::ToStringUtf(const String& string) {
  if (!ok() || !string) return "";
  Local<Array<uint8_t>> bytes = string.GetBytes(*this, String::GetUtf8());
  if (!ok() || !bytes) return "";
  size_t len = GetArrayLength(bytes);
  if (!ok() || len == 0) return "";
  std::string result(len, '\0');
  GetArrayRegion(bytes, 0, len, reinterpret_cast<uint8_t*>(&result[0]));
  return ok() ? result : "";
}

std::string Env::ErrorDescription(const Object& object) {
  if (!object) return "(null)";
  ExceptionClearGuard block(*this);
  std::string result = object.ToString(*this);
  if (ok()) return result;
  Local<Throwable> exception = ClearExceptionOccurred();
  std::string message = exception.GetMessage(*this);
  ExceptionClear();
  return "(unknown object: failed trying to describe it: " + message + ")";
}

const char* Env::ErrorName(jint error) {
  switch (error) {
    case JNI_OK: return "no error (JNI_OK)";
    case JNI_ERR: return "general JNI failure (JNI_ERR)";
    case JNI_EDETACHED: return "thread detached from the VM (JNI_EDETACHED)";
    case JNI_EVERSION: return "JNI version error (JNI_EVERSION)";
    case JNI_ENOMEM: return "not enough memory (JNI_ENOMEM)";
    case JNI_EEXIST: return "VM already created (JNI_EEXIST)";
    case JNI_EINVAL: return "invalid arguments (JNI_EINVAL)";
    default: return "unexpected error code";
  }
}

}  // namespace firebase::firestore::jni
