#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LOADER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LOADER_H_

#include <jni.h>

#include <vector>

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"

// To ensure that Proguard doesn't strip the classes you're using, place this
// string directly before the JNI class string in your Class global
// declarations.
#define PROGUARD_KEEP_CLASS "%PG%"

namespace firebase {
class App;

namespace firestore {
namespace jni {

/**
 * Loads cacheable JNI objects including classes, methods, and fields.
 * Automatically unloads any loaded classes when destroyed.
 */
class Loader {
 public:
  explicit Loader(App* app);
  ~Loader();

  // Loader is move-only
  Loader(const Loader&) = delete;
  Loader& operator=(const Loader&) = delete;

  Loader(Loader&&) = default;
  Loader& operator=(Loader&&) = default;

  /**
   * Returns true if the loader has succeeded. If not, any errors have already
   * been logged.
   */
  bool ok() const { return ok_; }

  /**
   * Loads a Java class described by the given class name. The class name as
   * would be passed to `JNIEnv::FindClass`, e.g. `"java/util/String"`.
   */
  void LoadClass(const char* name);

  /**
   * Loads a Java constructor from the last loaded class.
   */
  void Load(ConstructorBase& method);

  /**
   * Loads a Java instance method from the last loaded class.
   */
  void Load(MethodBase& method);

  /**
   * Loads a Java static field from the last loaded class.
   */
  void Load(StaticFieldBase& field);

  /**
   * Loads a Java static method from the last loaded class.
   */
  void Load(StaticMethodBase& method);

  void Unload();

 private:
  App* app_ = nullptr;
  JNIEnv* env_ = nullptr;

  const char* last_class_name_ = nullptr;
  jclass last_class_ = nullptr;

  bool ok_ = true;

  // A list of classes that were successfully loaded. This is held as a
  // UniquePtr to allow Loader to be move-only when built with STLPort.
  std::vector<jclass> loaded_classes_;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LOADER_H_
