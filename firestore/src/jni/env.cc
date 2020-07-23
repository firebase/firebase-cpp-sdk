#include "firestore/src/jni/env.h"

#include "app/src/assert.h"
#include "app/src/log.h"

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
#if !__cpp_exceptions
  return 0;

#elif __cplusplus >= 201703L
  return std::uncaught_exceptions();

#else
  return std::uncaught_exception() ? 1 : 0;
#endif  // !__cpp_exceptions
}

}  // namespace

Env::Env() : Env(GetEnv()) {}

Env::Env(JNIEnv* env)
    : env_(env), initial_pending_exceptions_(CurrentExceptionCount()) {}

Env::~Env() noexcept(false) {
  if (ok()) return;

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
  if (!ok()) return {};

  jclass result = env_->FindClass(name);
  RecordException();
  return Local<Class>(env_, result);
}

void Env::Throw(const Throwable& throwable) {
  if (!ok()) return;

  jint result = env_->Throw(throwable.get());
  FIREBASE_ASSERT_MESSAGE(
      result == JNI_OK, "Failed to throw an exception %s: %s",
      ErrorDescription(throwable).c_str(), ErrorName(result));
}

void Env::ThrowNew(const Class& clazz, const char* message) {
  if (!ok()) return;

  jint result = env_->ThrowNew(clazz.get(), message);
  FIREBASE_ASSERT_MESSAGE(
      result == JNI_OK, "Failed to throw %s with message %s: %s",
      ErrorDescription(clazz).c_str(), message, ErrorName(result));
}

Local<Throwable> Env::ExceptionOccurred() {
  jthrowable exception = env_->ExceptionOccurred();
  return Local<Throwable>(env_, exception);
}

void Env::ExceptionClear() { env_->ExceptionClear(); }

Local<Throwable> Env::ClearExceptionOccurred() {
  Local<Throwable> result = ExceptionOccurred();
  ExceptionClear();
  return result;
}

Local<Class> Env::GetObjectClass(const Object& object) {
  if (!ok()) return {};

  jclass result = env_->GetObjectClass(object.get());
  RecordException();
  return Local<Class>(env_, result);
}

bool Env::IsInstanceOf(const Object& object, const Class& clazz) {
  if (!ok()) return false;

  jboolean result = env_->IsInstanceOf(object.get(), clazz.get());
  RecordException();
  return result;
}

bool Env::IsSameObject(const Object& object1, const Object& object2) {
  if (!ok()) return false;

  jboolean result = env_->IsSameObject(object1.get(), object2.get());
  RecordException();
  return result;
}

jmethodID Env::GetMethodId(const Class& clazz, const char* name,
                           const char* sig) {
  if (!ok()) return nullptr;

  jmethodID result = env_->GetMethodID(clazz.get(), name, sig);
  RecordException();
  return result;
}

jfieldID Env::GetStaticFieldId(const Class& clazz, const char* name,
                               const char* sig) {
  if (!ok()) return nullptr;

  jfieldID result = env_->GetStaticFieldID(ToJni(clazz), name, sig);
  RecordException();
  return result;
}

jmethodID Env::GetStaticMethodId(const Class& clazz, const char* name,
                                 const char* sig) {
  if (!ok()) return nullptr;

  jmethodID result = env_->GetStaticMethodID(ToJni(clazz), name, sig);
  RecordException();
  return result;
}

Local<String> Env::NewStringUtf(const char* bytes) {
  if (!ok()) return {};

  jstring result = env_->NewStringUTF(bytes);
  RecordException();
  return Local<String>(env_, result);
}

std::string Env::GetStringUtfRegion(const String& string, size_t start,
                                    size_t len) {
  if (!ok()) return "";

  // Copy directly into the std::string buffer. This is guaranteed to work as
  // of C++11, and also happens to work with STLPort.
  std::string result;
  result.resize(len);

  env_->GetStringUTFRegion(string.get(), ToJni(start), ToJni(len), &result[0]);
  RecordException();

  // Ensure that if there was an exception, the contents of the buffer are
  // disregarded.
  if (!ok()) return "";
  return result;
}

void Env::RecordException() {
  if (ok()) return;

  env_->ExceptionDescribe();
}

std::string Env::ErrorDescription(const Object& object) {
  ExceptionClearGuard block(*this);
  std::string result = object.ToString(*this);
  if (ok()) {
    return result;
  }

  auto exception = ExceptionOccurred();

  ExceptionClearGuard block2(*this);
  std::string message = exception.GetMessage(*this);
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
