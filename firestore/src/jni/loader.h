#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LOADER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LOADER_H_

#include <jni.h>

#include <string>
#include <vector>

#include "app/meta/move.h"
#include "app/src/embedded_file.h"
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
  using EmbeddedFile = firebase::internal::EmbeddedFile;

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
   * Adds metadata about embedded class files in the binary distribution.
   */
  void AddEmbeddedFile(const char* name, const unsigned char* data,
                       size_t size);
  /**
   * Unpacks any embedded files added above and writes them out to a temporary
   * location. `Load(Class&)` will search these files for classes (in addition
   * to the standard classpath).
   */
  void CacheEmbeddedFiles();

  // TODO(mcg): remove once InitializeEmbeddedClasses instances are gone.
  const std::vector<EmbeddedFile>* embedded_files() const {
    return &embedded_files_;
  }

  /**
   * Uses the given class reference as the basis for subsequent loads. The
   * caller still owns the class reference and the Loader will not clean it up.
   *
   * @param class_name The class name the reference represents, in the form
   *     "java/lang/String".
   * param existing_ref An existing local or global reference to a Java class.
   */
  void UsingExistingClass(const char* class_name, jclass existing_ref);

  /**
   * Uses the given class reference for loading the given members. The caller
   * still owns the class reference and the Loader will not clean it up.
   *
   * @param class_name The class name the reference represents, in the form
   *     "java/lang/String".
   * param existing_ref An existing local or global reference to a Java class.
   */
  template <typename... Members>
  void LoadFromExistingClass(const char* class_name, jclass existing_ref,
                             Members&&... members) {
    UsingExistingClass(class_name, existing_ref);
    LoadAll(Forward<Members>(members)...);
  }

  /**
   * Loads a Java class described by the given class name. The class name as
   * would be passed to `JNIEnv::FindClass`, e.g. `"java/util/String"`.
   */
  jclass LoadClass(const char* class_name);

  /**
   * Loads a Java class and all its members in a single invocation.
   */
  template <typename... Members>
  jclass LoadClass(const char* name, Members&&... members) {
    jclass result = LoadClass(name);
    LoadAll(Forward<Members>(members)...);
    return result;
  }

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

  /**
   * Loads all the given members by calling the appropriate `Load` overload.
   */
  template <typename Member, typename... Members>
  void LoadAll(Member&& first, Members&&... rest) {
    Load(Forward<Member>(first));
    LoadAll(Forward<Members>(rest)...);
  }
  void LoadAll() {}

  /**
   * Registers the given native methods with the JVM.
   */
  bool RegisterNatives(const JNINativeMethod methods[], size_t num_methods);

  void Unload();

 private:
  App* app_ = nullptr;
  JNIEnv* env_ = nullptr;

  std::string last_class_name_;
  jclass last_class_ = nullptr;

  bool ok_ = true;

  // A list of classes that were successfully loaded. This is held as a
  // UniquePtr to allow Loader to be move-only when built with STLPort.
  std::vector<jclass> loaded_classes_;

  // A list of embedded files from which to load classes
  std::vector<EmbeddedFile> embedded_files_;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_LOADER_H_
