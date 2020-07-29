#include "firestore/src/jni/loader.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/declaration.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr const char* StripProguardPrefix(const char* class_name) {
  return class_name[0] == '%' ? &class_name[4] : class_name;
}

}  // namespace

Loader::Loader(App* app) : app_(app), env_(app->GetJNIEnv()) {}

Loader::~Loader() { Unload(); }

void Loader::LoadClass(const char* name) {
  if (!ok_) return;

  name = StripProguardPrefix(name);
  last_class_name_ = name;
  last_class_ = util::FindClassGlobal(env_, app_->activity(), nullptr, name,
                                      util::kClassRequired);
  if (!last_class_) {
    ok_ = false;
    return;
  }

  loaded_classes_.push_back(last_class_);
}

void Loader::Load(ConstructorBase& ctor) {
  if (!ok_) return;

  util::MethodNameSignature descriptor = {
      "<init>", ctor.sig_, util::kMethodTypeInstance, util::kMethodRequired};

  jmethodID result = nullptr;
  ok_ = util::LookupMethodIds(env_, last_class_, &descriptor,
                              /*number_of_method_name_signatures=*/1u, &result,
                              last_class_name_);
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
                              last_class_name_);
  if (!ok_) return;

  method.id_ = result;
}

void Loader::Load(StaticFieldBase& field) {
  if (!ok_) return;

  util::FieldDescriptor descriptor = {
      field.name_, field.sig_, util::kFieldTypeStatic, util::kMethodRequired};

  jfieldID result = nullptr;
  ok_ = util::LookupFieldIds(env_, last_class_, &descriptor, 1u, &result,
                             last_class_name_);
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
                              last_class_name_);
  if (!ok_) return;

  method.clazz_ = last_class_;
  method.id_ = result;
}

void Loader::Unload() {
  JNIEnv* env = app_->GetJNIEnv();
  for (jclass clazz : loaded_classes_) {
    env->DeleteGlobalRef(clazz);
  }
  loaded_classes_.clear();
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
