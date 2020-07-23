#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {
namespace jni {

Local<Class> Env::FindClass(const char* name) {
  jclass result = env_->FindClass(name);
  RecordException();
  return Local<Class>(env_, result);
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
  if (last_exception_ || !env_->ExceptionCheck()) return;

  env_->ExceptionDescribe();

  last_exception_ = env_->ExceptionOccurred();
  env_->ExceptionClear();
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
