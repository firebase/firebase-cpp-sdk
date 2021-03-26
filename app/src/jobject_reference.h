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
#ifndef FIREBASE_APP_CLIENT_CPP_SRC_JOBJECT_REFERENCE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_JOBJECT_REFERENCE_H_

#include <jni.h>

#include "app/src/include/firebase/internal/common.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace internal {

// Creates an alias of internal::JObjectReference named classname.
// This is useful when defining the implementation of a forward declared class
// using JObjectReference.
#define JOBJECT_REFERENCE(classname)                                          \
  class classname : public FIREBASE_NAMESPACE::internal::JObjectReference {   \
   public:                                                                    \
     explicit classname(JNIEnv *env) :                                        \
         FIREBASE_NAMESPACE::internal::JObjectReference(env) {}               \
     explicit classname(                                                      \
         const FIREBASE_NAMESPACE::internal::JObjectReference& obj) :         \
         FIREBASE_NAMESPACE::internal::JObjectReference(obj) {}               \
     explicit classname(                                                      \
           FIREBASE_NAMESPACE::internal::JObjectReference&& obj) :            \
         FIREBASE_NAMESPACE::internal::JObjectReference(obj) {}               \
     classname(JNIEnv *env, jobject obj) :                                    \
         FIREBASE_NAMESPACE::internal::JObjectReference(env, obj) {}          \
     classname& operator=(                                                    \
         const FIREBASE_NAMESPACE::internal::JObjectReference& rhs) {         \
       FIREBASE_NAMESPACE::internal::JObjectReference::operator=(rhs);        \
       return *this;                                                          \
     }                                                                        \
     classname& operator=(                                                    \
           FIREBASE_NAMESPACE::internal::JObjectReference&& rhs) {            \
       FIREBASE_NAMESPACE::internal::JObjectReference::operator=(rhs);        \
       return *this;                                                          \
     }                                                                        \
  }

// Creates and holds a global reference to a Java object.
class JObjectReference {
 public:
  JObjectReference();
  explicit JObjectReference(JNIEnv* env);
  // Create a reference to a java object.
  JObjectReference(JNIEnv* env, jobject object);
  // Copy
  JObjectReference(const JObjectReference& reference);
  // Move
#ifdef FIREBASE_USE_MOVE_OPERATORS
  JObjectReference(JObjectReference&& reference) noexcept;
#endif  // FIREBASE_USE_MOVE_OPERATORS
  // Delete the reference to the java object.
  ~JObjectReference();
  // Copy this reference.
  JObjectReference& operator=(const JObjectReference& reference);
  // Move this reference.
#ifdef FIREBASE_USE_MOVE_OPERATORS
  JObjectReference& operator=(JObjectReference&& reference) noexcept;
#endif  // FIREBASE_USE_MOVE_OPERATORS

  // Add a global reference to the specified object, removing the reference
  // to the object currently referenced by this class.  If jobject_reference
  // is null, the existing reference is removed.
  void Set(jobject jobject_reference);

  void Set(JNIEnv* env, jobject jobject_reference);

  // Get a JNIEnv from the JavaVM associated with this class.
  JNIEnv* GetJNIEnv() const;

  // Get the JavaVM associated with this class.
  JavaVM* java_vm() const { return java_vm_; }

  // Get the global reference to the Java object without incrementing the
  // reference count.
  jobject object() const { return object_; }

  // Get a local reference to the object. The returned reference must be
  // deleted after use with DeleteLocalRef().
  jobject GetLocalRef() const;

  // Same as object()
  jobject operator*() const { return object(); }

  // Convert a local reference to a JObjectReference, deleting the local
  // reference.
  static JObjectReference FromLocalReference(JNIEnv* env,
                                             jobject local_reference);

 private:
  // Initialize this instance by adding a reference to the specified Java
  // object.
  void Initialize(JavaVM* jvm, JNIEnv* env, jobject jobject_reference);
  // Get JavaVM from a JNIEnv.
  static JavaVM* GetJavaVM(JNIEnv* env);

 private:
  JavaVM* java_vm_;
  jobject object_;
};

}  // namespace internal
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_JOBJECT_REFERENCE_H_
