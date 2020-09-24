#include "firestore/src/jni/loader.h"

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/declaration.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr const char* StripProguardPrefix(const char* class_name) {
  return class_name[0] == '%' ? &class_name[4] : class_name;
}

}  // namespace

Loader::Loader(App* app) : app_(app), env_(app->GetJNIEnv()) {
  Class::Initialize(*this);
}

Loader::~Loader() { Unload(); }

void Loader::AddEmbeddedFile(const char* name, const unsigned char* data,
                             size_t size) {
#if defined(_STLPORT_VERSION)
  embedded_files_.push_back(EmbeddedFile(name, data, size));
#else
  embedded_files_.emplace_back(name, data, size);
#endif
}

void Loader::CacheEmbeddedFiles() {
  if (!ok_) return;

  util::CacheEmbeddedFiles(env_, app_->activity(), embedded_files_);
}

void Loader::UsingExistingClass(const char* class_name, jclass existing_ref) {
  if (!ok_) return;

  last_class_name_ = class_name;
  last_class_ = existing_ref;
}

jclass Loader::LoadClass(const char* class_name) {
  if (!ok_) return nullptr;

  class_name = StripProguardPrefix(class_name);
  last_class_name_ = class_name;
  last_class_ = util::FindClassGlobal(env_, app_->activity(), &embedded_files_,
                                      class_name, util::kClassRequired);
  if (!last_class_) {
    ok_ = false;
    return nullptr;
  }

  loaded_classes_.push_back(last_class_);
  return last_class_;
}

void Loader::Load(ConstructorBase& ctor) {
  if (!ok_) return;

  util::MethodNameSignature descriptor = {
      "<init>", ctor.sig_, util::kMethodTypeInstance, util::kMethodRequired};

  jmethodID result = nullptr;
  ok_ = util::LookupMethodIds(env_, last_class_, &descriptor,
                              /*number_of_method_name_signatures=*/1u, &result,
                              last_class_name_.c_str());
  if (!ok_) return;

  ctor.clazz_ = last_class_;
  ctor.id_ = result;
}

void Loader::Load(MethodBase& method) {
  if (!ok_) return;

  util::MethodNameSignature descriptor = {method.name_, method.sig_,
                                          util::kMethodTypeInstance,
                                          util::kMethodRequired};

  jmethodID result = nullptr;
  ok_ = util::LookupMethodIds(env_, last_class_, &descriptor, 1u, &result,
                              last_class_name_.c_str());
  if (!ok_) return;

  method.id_ = result;
}

void Loader::Load(StaticFieldBase& field) {
  if (!ok_) return;

  util::FieldDescriptor descriptor = {
      field.name_, field.sig_, util::kFieldTypeStatic, util::kMethodRequired};

  jfieldID result = nullptr;
  ok_ = util::LookupFieldIds(env_, last_class_, &descriptor, 1u, &result,
                             last_class_name_.c_str());
  if (!ok_) return;

  field.clazz_ = last_class_;
  field.id_ = result;
}

void Loader::Load(StaticMethodBase& method) {
  if (!ok_) return;

  util::MethodNameSignature descriptor = {method.name_, method.sig_,
                                          util::kMethodTypeStatic,
                                          util::kMethodRequired};

  jmethodID result = nullptr;
  ok_ = util::LookupMethodIds(env_, last_class_, &descriptor, 1u, &result,
                              last_class_name_.c_str());
  if (!ok_) return;

  method.clazz_ = last_class_;
  method.id_ = result;
}

bool Loader::RegisterNatives(const JNINativeMethod methods[],
                             size_t num_methods) {
  if (!ok_) return false;

  auto* true_native_methods = ConvertJNINativeMethod(methods, num_methods);
  jint result =
      env_->RegisterNatives(last_class_, true_native_methods, num_methods);
  CleanUpConvertedJNINativeMethod(true_native_methods);

  if (result != JNI_OK) {
    ok_ = false;
  }
  return ok_;
}

void Loader::Unload() {
  if (loaded_classes_.empty()) return;

  JNIEnv* env = GetEnv();
  for (jclass clazz : loaded_classes_) {
    env->DeleteGlobalRef(clazz);
  }
  loaded_classes_.clear();
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
