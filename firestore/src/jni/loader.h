// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_LOADER_H_
#define FIREBASE_FIRESTORE_SRC_JNI_LOADER_H_

#include <jni.h>

#include <string>
#include <utility>
#include <vector>

#include "app/src/embedded_file.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"

#define PROGUARD_KEEP_CLASS "%PG%"

namespace firebase {
class App;

namespace firestore::jni {

class Loader {
 public:
  using EmbeddedFile = firebase::internal::EmbeddedFile;

  explicit Loader(App* app);
  ~Loader();

  Loader(const Loader&) = delete;
  Loader& operator=(const Loader&) = delete;

  Loader(Loader&& other) noexcept { *this = std::move(other); }

  Loader& operator=(Loader&& other) noexcept {
    if (this != &other) {
      Unload();
      app_ = std::exchange(other.app_, nullptr);
      env_ = std::exchange(other.env_, nullptr);
      last_class_name_ = std::move(other.last_class_name_);
      last_class_ = std::exchange(other.last_class_, nullptr);
      ok_ = std::exchange(other.ok_, false);
      loaded_classes_ = std::move(other.loaded_classes_);
      other.loaded_classes_.clear();
      embedded_files_ = std::move(other.embedded_files_);
      other.embedded_files_.clear();
    }
    return *this;
  }

  bool ok() const { return ok_ && env_ != nullptr && !env_->ExceptionCheck(); }

  void AddEmbeddedFile(const char* name, const unsigned char* data,
                       size_t size);
  void CacheEmbeddedFiles();
  const std::vector<EmbeddedFile>* embedded_files() const {
    return &embedded_files_;
  }

  void UsingExistingClass(const char* class_name, jclass existing_ref);

  template <typename... Members>
  void LoadFromExistingClass(const char* class_name, jclass existing_ref,
                             Members&&... members) {
    UsingExistingClass(class_name, existing_ref);
    (Load(std::forward<Members>(members)), ...);
  }

  jclass LoadClass(const char* class_name);

  template <typename... Members>
  jclass LoadClass(const char* name, Members&&... members) {
    jclass result = LoadClass(name);
    (Load(std::forward<Members>(members)), ...);
    return result;
  }

  void Load(ConstructorBase& method);
  void Load(MethodBase& method);
  void Load(StaticFieldBase& field);
  void Load(StaticMethodBase& method);

  template <typename... Members>
  void LoadAll(Members&&... members) {
    (Load(std::forward<Members>(members)), ...);
  }

  bool RegisterNatives(const JNINativeMethod methods[], size_t num_methods);
  void Unload();
  JNIEnv* env() { return env_; }

 private:
  App* app_ = nullptr;
  JNIEnv* env_ = nullptr;
  std::string last_class_name_;
  jclass last_class_ = nullptr;
  bool ok_ = true;
  std::vector<jclass> loaded_classes_;
  std::vector<EmbeddedFile> embedded_files_;
};

}  // namespace firestore::jni
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_LOADER_H_
