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

#include "firestore/src/jni/env.h"

#include <cstdint>
#include <cstring>
#include <exception>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "firestore/src/jni/array.h"

// Add constant declarations missing from the NDK's jni.h
#ifndef JNI_ENOMEM
#define JNI_ENOMEM -4
#define JNI_EEXIST -5
#define JNI_EINVAL -6
#endif

namespace firebase {
namespace firestore {
namespace jni {
namespace {

/**
 * Returns the number of currently pending exceptions. This can be more than
 * one if an exception is thrown in a try-catch block in a destructor. Returns
 * zero if exceptions are disabled.
 */
int CurrentExceptionCount() {
#if __cpp_exceptions
  return std::uncaught_exceptions();
#else
  return 0;
#endif  // __cpp_exceptions
}

}  // namespace

Env::Env() : Env(GetEnv()) {}

Env::Env(JNIEnv* env)
    : env_(env), initial_pending_exceptions_(CurrentExceptionCount()) {}

Env::~Env() noexcept(false) {
  // Avoid calling ok() if there is no registered exception handler. It is
  // slightly more efficient in the common case that no exception handler is
  // specified, and also avoids calling into the JVM in the rare case that the
  // thread is already detached.
  if (!exception_handler_ || !env_ || ok()) {
    return;
  }

  if (exception_handler_ &&
      CurrentExceptionCount() == initial_pending_exceptions_) {
    exception_handler_(*this, ExceptionOccurred(), context_);
  }

  // If no unhandled exception handler is registered, leave the exception
  // pending in the JNIEnv. This will either propagate out to another Env
  // instance that does have a handler installed or will propagate out to the
  // JVM.
}

Local<Class> Env::FindClass(const char* name) {
  if (!ok() || !name) return {};

  jclass result = env_->FindClass(name);
  RecordException();
  return Local<Class>(env_, result);
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
  if (!env_) return {};
  jthrowable exception = env_->ExceptionOccurred();
  return Local<Throwable>(env_, exception);
}

void Env::ExceptionClear() {
  if (!env_) return;
  env_->ExceptionClear();
}

Local<Throwable> Env::ClearExceptionOccurred() {
  if (!env_) return {};
  Local<Throwable> result = ExceptionOccurred();
  ExceptionClear();
  return result;
}

Local<Class> Env::GetObjectClass(const Object& object) {
  if (!ok() || !object) return {};

  jclass result = env_->GetObjectClass(object.get());
  RecordException();
  return Local<Class>(env_, result);
}

bool Env::IsInstanceOf(const Object& object, const Class& clazz) {
  if (!ok() || !object || !clazz) return false;

  jboolean result = env_->IsInstanceOf(object.get(), clazz.get());
  RecordException();
  return result != JNI_FALSE;
}

bool Env::IsSameObject(const Object& object1, const Object& object2) {
  if (!ok()) return false;

  jboolean result = env_->IsSameObject(object1.get(), object2.get());
  RecordException();
  return result != JNI_FALSE;
}

jmethodID Env::GetMethodId(const Class& clazz,
                           const char* name,
                           const char* sig) {
  if (!ok() || !clazz || !name || !sig) return nullptr;

  jmethodID result = env_->GetMethodID(clazz.get(), name, sig);
  RecordException();
  return result;
}

jfieldID Env::GetStaticFieldId(const Class& clazz,
                               const char* name,
                               const char* sig) {
  if (!ok() || !clazz || !name || !sig) return nullptr;

  jfieldID result = env_->GetStaticFieldID(ToJni(clazz), name, sig);
  RecordException();
  return result;
}

jmethodID Env::GetStaticMethodId(const Class& clazz,
                                 const char* name,
                                 const char* sig) {
  if (!ok() || !clazz || !name || !sig) return nullptr;

  jmethodID result = env_->GetStaticMethodID(ToJni(clazz), name, sig);
  RecordException();
  return result;
}

Local<String> Env::NewStringUtf(const char* bytes) {
  if (!bytes) return {};

  return NewStringUtf(bytes, std::strlen(bytes));
}

Local<String> Env::NewStringUtf(const char* bytes, size_t size) {
  if (!ok() || (!bytes && size > 0)) return {};

  Local<Array<uint8_t>> java_bytes = NewArray<uint8_t>(size);
  if (!ok() || !java_bytes) return {};

  SetArrayRegion(java_bytes, 0, size, reinterpret_cast<const uint8_t*>(bytes));
  if (!ok()) return {};

  return String::Create(*this, java_bytes, String::GetUtf8());
}

std::string Env::ToStringUtf(const String& string) {
  if (!ok() || !string) return "";

  Local<Array<uint8_t>> bytes = string.GetBytes(*this, String::GetUtf8());
  if (!ok() || !bytes) return "";

  size_t len = GetArrayLength(bytes);
  if (!ok() || len == 0) return "";

  // Copy directly into the std::string buffer. This is guaranteed to work as
  // of C++11.
  std::string result(len, '\0');
  auto* buffer = reinterpret_cast<uint8_t*>(&result[0]);
  GetArrayRegion(bytes, 0, len, buffer);
  if (!ok()) return "";

  return result;
}

void Env::RecordException() {
  if (!env_ || ok()) return;
}

std::string Env::ErrorDescription(const Object& object) {
  if (!object) {
    return "(null)";
  }

  ExceptionClearGuard block(*this);
  std::string result = object.ToString(*this);
  if (ok()) {
    return result;
  }

  Local<Throwable> exception = ClearExceptionOccurred();
  std::string message = exception.GetMessage(*this);
  ExceptionClear();
  return std::string("(unknown object: failed trying to describe it: ") +
         message + ")";
}

const char* Env::ErrorName(jint error) {
  switch (error) {
    case JNI_OK:
      return "no error (JNI_OK)";
    case JNI_ERR:
      return "general JNI failure (JNI_ERR)";
    case JNI_EDETACHED:
      return "thread detached from the VM (JNI_EDETACHED)";
    case JNI_EVERSION:
      return "JNI version error (JNI_EVERSION)";
    case JNI_ENOMEM:
      return "not enough memory (JNI_ENOMEM)";
    case JNI_EEXIST:
      return "VM already created (JNI_EEXIST)";
    case JNI_EINVAL:
      return "invalid arguments (JNI_EINVAL)";
    default:
      return "unexpected error code";
  }
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
