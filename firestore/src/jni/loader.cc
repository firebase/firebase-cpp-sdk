// Copyright 2020 Google LLC

#include "firestore/src/jni/loader.h"

#include <cstring>
#include <limits>
#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/declaration.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {
namespace {

const char* StripProguardPrefix(const char* class_name) {
  if (!class_name) return nullptr;
  constexpr size_t kPrefixLen = sizeof(PROGUARD_KEEP_CLASS) - 1;
  return std::strncmp(class_name, PROGUARD_KEEP_CLASS, kPrefixLen) == 0
             ? class_name + kPrefixLen
             : class_name;
}

jmethodID LookupMethod(JNIEnv* env, jclass clazz, const char* class_name,
                       const char* name, const char* sig,
                       firebase::util::MethodType type, bool* ok) {
  if (!clazz || !name || !sig) {
    *ok = false;
    return nullptr;
  }
  firebase::util::MethodNameSignature descriptor = {
      name, sig, type, firebase::util::kMethodRequired};
  jmethodID result = nullptr;
  *ok = firebase::util::LookupMethodIds(env, clazz, &descriptor, 1u, &result,
                                        class_name);
  return *ok ? result : nullptr;
}

}  // namespace

Loader::Loader(App* app)
    : app_(app), env_(app ? app->GetJNIEnv() : GetEnv()), ok_(env_ != nullptr) {
  Class::Initialize(*this);
  Object::Initialize(*this);
}

Loader::~Loader() { Unload(); }

void Loader::AddEmbeddedFile(const char* name, const unsigned char* data,
                             size_t size) {
  embedded_files_.emplace_back(name, data, size);
}

void Loader::CacheEmbeddedFiles() {
  if (!ok()) return;
  if (!app_) {
    ok_ = false;
    return;
  }
  firebase::util::CacheEmbeddedFiles(env_, app_->activity(), embedded_files_);
  if (env_->ExceptionCheck()) {
    env_->ExceptionClear();
    ok_ = false;
  }
}

void Loader::UsingExistingClass(const char* class_name, jclass existing_ref) {
  if (!ok()) return;
  class_name = StripProguardPrefix(class_name);
  if (!class_name || !existing_ref) {
    ok_ = false;
    last_class_ = nullptr;
    last_class_name_.clear();
    return;
  }
  last_class_name_ = class_name;
  last_class_ = existing_ref;
}

jclass Loader::LoadClass(const char* class_name) {
  if (!ok()) return nullptr;
  class_name = StripProguardPrefix(class_name);
  if (!class_name || !app_) {
    ok_ = false;
    last_class_ = nullptr;
    last_class_name_.clear();
    return nullptr;
  }
  last_class_name_ = class_name;
  last_class_ = firebase::util::FindClassGlobal(
      env_, app_->activity(), &embedded_files_, class_name,
      firebase::util::kClassRequired);
  if (!last_class_) {
    ok_ = false;
    return nullptr;
  }
  loaded_classes_.push_back(last_class_);
  return last_class_;
}

void Loader::Load(ConstructorBase& ctor) {
  if (!ok()) return;
  jmethodID result = LookupMethod(env_, last_class_, last_class_name_.c_str(),
                                  "<init>", ctor.sig_,
                                  firebase::util::kMethodTypeInstance, &ok_);
  if (ok_) {
    ctor.clazz_ = last_class_;
    ctor.id_ = result;
  }
}

void Loader::Load(MethodBase& method) {
  if (!ok()) return;
  jmethodID result = LookupMethod(env_, last_class_, last_class_name_.c_str(),
                                  method.name_, method.sig_,
                                  firebase::util::kMethodTypeInstance, &ok_);
  if (ok_) method.id_ = result;
}

void Loader::Load(StaticFieldBase& field) {
  if (!ok()) return;
  if (!last_class_ || !field.name_ || !field.sig_) {
    ok_ = false;
    return;
  }
  firebase::util::FieldDescriptor descriptor = {
      field.name_, field.sig_, firebase::util::kFieldTypeStatic,
      firebase::util::kMethodRequired};
  jfieldID result = nullptr;
  ok_ = firebase::util::LookupFieldIds(env_, last_class_, &descriptor, 1u,
                                       &result, last_class_name_.c_str());
  if (ok_) {
    field.clazz_ = last_class_;
    field.id_ = result;
  }
}

void Loader::Load(StaticMethodBase& method) {
  if (!ok()) return;
  jmethodID result = LookupMethod(env_, last_class_, last_class_name_.c_str(),
                                  method.name_, method.sig_,
                                  firebase::util::kMethodTypeStatic, &ok_);
  if (ok_) {
    method.clazz_ = last_class_;
    method.id_ = result;
  }
}

bool Loader::RegisterNatives(const JNINativeMethod methods[],
                             size_t num_methods) {
  if (!ok()) return false;
  if (!last_class_ || (!methods && num_methods > 0) ||
      num_methods > static_cast<size_t>(std::numeric_limits<jint>::max())) {
    ok_ = false;
    return false;
  }
  if (num_methods == 0) return true;
  auto* true_methods =
      ConvertJNINativeMethod(methods, static_cast<jint>(num_methods));
  jint result = env_->RegisterNatives(last_class_, true_methods,
                                      static_cast<jint>(num_methods));
  CleanUpConvertedJNINativeMethod(true_methods);
  if (result != JNI_OK || env_->ExceptionCheck()) {
    if (env_->ExceptionCheck()) {
      env_->ExceptionDescribe();
      env_->ExceptionClear();
    }
    ok_ = false;
  }
  return ok_;
}

void Loader::Unload() {
  last_class_ = nullptr;
  last_class_name_.clear();
  if (loaded_classes_.empty()) return;
  if (JNIEnv* env = GetEnv()) {
    for (jclass clazz : loaded_classes_) {
      if (clazz) env->DeleteGlobalRef(clazz);
    }
  }
  loaded_classes_.clear();
}

}  // namespace firebase::firestore::jni
