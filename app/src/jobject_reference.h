/*
 * Copyright 2019 Google LLC
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

#ifndef FIREBASE_APP_SRC_JOBJECT_REFERENCE_H_
#define FIREBASE_APP_SRC_JOBJECT_REFERENCE_H_

#include <jni.h>

#include <atomic>
#include <utility>

/// @brief Macro to create a child class of JObjectReference for forward
/// declaration compatibility.
#define JOBJECT_REFERENCE(classname)                                        \
  class classname : public ::firebase::internal::JObjectReference {         \
   public:                                                                  \
    classname() = default;                                                  \
    explicit classname(JNIEnv* env)                                         \
        : ::firebase::internal::JObjectReference(env) {}                    \
    classname(JNIEnv* env, jobject obj)                                     \
        : ::firebase::internal::JObjectReference(env, obj) {}               \
    classname(const classname&) = default;                                  \
    classname(classname&&) noexcept = default;                              \
    explicit classname(const ::firebase::internal::JObjectReference& obj)   \
        : ::firebase::internal::JObjectReference(obj) {}                    \
    explicit classname(::firebase::internal::JObjectReference&& obj) noexcept \
        : ::firebase::internal::JObjectReference(std::move(obj)) {}         \
    classname& operator=(const classname&) = default;                       \
    classname& operator=(classname&&) noexcept = default;                   \
    classname& operator=(                                                   \
        const ::firebase::internal::JObjectReference& rhs) {                \
      ::firebase::internal::JObjectReference::operator=(rhs);               \
      return *this;                                                         \
    }                                                                       \
    classname& operator=(                                                   \
        ::firebase::internal::JObjectReference&& rhs) noexcept {            \
      ::firebase::internal::JObjectReference::operator=(std::move(rhs));    \
      return *this;                                                         \
    }                                                                       \
  }

namespace firebase {
/// @cond FIREBASE_APP_INTERNAL
namespace internal {

// Backwards-compatible wrapper around a jobject global reference that manages
// creation and deletion of the global reference.
class JObjectReference {
 public:
  // Construct an empty reference with no associated JavaVM.
  JObjectReference();

  // Construct an empty reference bound to the given JNIEnv's JavaVM.
  explicit JObjectReference(JNIEnv* env);

  // Construct a reference from an existing jobject, creating a global reference
  // if `object` is non-null.
  JObjectReference(JNIEnv* env, jobject object);

  // Copy constructor creates a new global reference to the referenced jobject.
  JObjectReference(const JObjectReference& reference);

  // Move constructor transfers ownership of the existing global reference.
  JObjectReference(JObjectReference&& reference) noexcept;

  // Destructor deletes the global reference if non-null.
  virtual ~JObjectReference();

  // Copy assignment creates a new global reference to the referenced jobject.
  JObjectReference& operator=(const JObjectReference& reference);

  // Move assignment transfers ownership of the referenced jobject.
  JObjectReference& operator=(JObjectReference&& reference) noexcept;

  // Set this instance's object reference, releasing any previous reference.
  void Set(jobject jobject_reference);

  // Set this instance's object reference and JavaVM from `env`, releasing any
  // previous reference.
  void Set(JNIEnv* env, jobject jobject_reference);

  // Access the global reference.
  jobject object() const noexcept { return object_; }
  jobject Get() const noexcept { return object(); }
  jobject operator*() const noexcept { return object(); }
  explicit operator bool() const noexcept { return object_ != nullptr; }

  // Access the cached JavaVM*.
  JavaVM* java_vm() const noexcept { return java_vm_; }

  // Retrieve the JNIEnv* for the current thread using the cached JavaVM*.
  JNIEnv* GetJNIEnv() const;

  // Retrieve a new local reference to the wrapped jobject, or nullptr if unset.
  // The caller is responsible for calling env->DeleteLocalRef() on the result.
  [[nodiscard]] jobject GetLocalRef() const;

  // Retrieve the JavaVM* associated with the specified JNIEnv*.
  static JavaVM* GetJavaVM(JNIEnv* env);

  // Construct a JObjectReference from a local reference, creating a global
  // reference and immediately deleting `local_reference`.
  static JObjectReference FromLocalReference(JNIEnv* env,
                                             jobject local_reference);

  // Creates a new global reference to `jobject_reference` (without deleting
  // `jobject_reference`), deletes any prior global reference stored in
  // `*target_jobject_reference` (which must be initialized to nullptr or a
  // valid global reference prior to calling), and writes and returns the new
  // global reference in `*target_jobject_reference`.
  static jobject Set(JNIEnv* env, jobject jobject_reference,
                     std::atomic<jobject>* target_jobject_reference);

 private:
  // Initialize this reference, creating a global reference to jobject_reference
  // if it is non-null.
  void Initialize(JNIEnv* env, JavaVM* jvm, jobject jobject_reference);

  JavaVM* java_vm_;
  jobject object_;
};

}  // namespace internal
/// @endcond
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_JOBJECT_REFERENCE_H_
