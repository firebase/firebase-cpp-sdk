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

#include "app/src/jobject_reference.h"

#include <jni.h>

#include <atomic>

#include "app/src/assert.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/ownership.h"
#include "app/src/util_android.h"

namespace firebase {
namespace internal {

JObjectReference::JObjectReference() : java_vm_(nullptr), object_(nullptr) {}

JObjectReference::JObjectReference(JNIEnv* env)
    : java_vm_(nullptr), object_(nullptr) {
  Initialize(nullptr, env, nullptr);
}

JObjectReference::JObjectReference(JNIEnv* env, jobject object)
    : java_vm_(nullptr), object_(nullptr) {
  Initialize(nullptr, env, object);
}

JObjectReference::JObjectReference(const JObjectReference& reference)
    : java_vm_(reference.java_vm_), object_(nullptr) {
  if (reference.object_ != nullptr) {
    Initialize(reference.java_vm_, nullptr, reference.object_);
  }
}

JObjectReference::JObjectReference(JObjectReference&& reference) noexcept
    : java_vm_(reference.java_vm_), object_(reference.object_) {
  reference.java_vm_ = nullptr;
  reference.object_ = nullptr;
}

JObjectReference::~JObjectReference() { Set(nullptr); }

JObjectReference& JObjectReference::operator=(
    const JObjectReference& reference) {
  if (this == &reference) return *this;
  if (!java_vm_ && reference.java_vm_) {
    java_vm_ = reference.java_vm_;
  }
  if (reference.object_ == nullptr) {
    Set(nullptr);
    return *this;
  }
  Set(nullptr, reference.object_);
  return *this;
}

JObjectReference& JObjectReference::operator=(
    JObjectReference&& reference) noexcept {
  if (this != &reference) {
    Set(nullptr);
    java_vm_ = reference.java_vm_;
    object_ = reference.object_;
    reference.java_vm_ = nullptr;
    reference.object_ = nullptr;
  }
  return *this;
}

void JObjectReference::Set(jobject jobject_reference) {
  if (jobject_reference == nullptr && object_ == nullptr) {
    return;
  }
  Set(nullptr, jobject_reference);
}

void JObjectReference::Set(JNIEnv* env, jobject jobject_reference) {
  if (env && !java_vm_) {
    java_vm_ = GetJavaVM(env);
  }
  if (jobject_reference != nullptr && object_ != nullptr && env != nullptr) {
    if (env->IsSameObject(jobject_reference, object_)) {
      return;
    }
  } else if (jobject_reference == object_) {
    return;
  }

  jobject new_object = nullptr;
  if (jobject_reference != nullptr) {
    new_object = jni::internal::GlobalRefReaper::NewGlobalRef(
        env, jobject_reference, java_vm_);
  }

  if (object_ != nullptr) {
    jni::internal::GlobalRefReaper::DeleteOrEnqueue(object_, env,
                                                    java_vm_);
  }

  object_ = new_object;
  if (java_vm_ == nullptr && env != nullptr) {
    java_vm_ = GetJavaVM(env);
  }
}



JObjectReference JObjectReference::FromLocalReference(
    JNIEnv* env, jobject local_reference) {
  JObjectReference jref(env, local_reference);
  if (env && local_reference) {
    env->DeleteLocalRef(local_reference);
  }
  return jref;
}

JNIEnv* JObjectReference::GetJNIEnv() const {
  JavaVM* vm =
      java_vm_ ? java_vm_ : jni::internal::GlobalRefReaper::GetJavaVM();
  return vm ? util::GetThreadsafeJNIEnv(vm) : nullptr;
}

jobject JObjectReference::GetLocalRef() const {
  JNIEnv* env = GetJNIEnv();
  return object_ && env ? env->NewLocalRef(object_) : nullptr;
}

void JObjectReference::Initialize(JavaVM* jvm, JNIEnv* env,
                                  jobject jobject_reference) {
  java_vm_ = jvm ? jvm : GetJavaVM(env);
  FIREBASE_DEV_ASSERT(env || java_vm_ || !jobject_reference);
  object_ = nullptr;
  if (jobject_reference != nullptr) {
    object_ = jni::internal::GlobalRefReaper::NewGlobalRef(
        env, jobject_reference, java_vm_);
  }
}

JavaVM* JObjectReference::GetJavaVM(JNIEnv* env) {
  if (!env) {
    return jni::internal::GlobalRefReaper::GetJavaVM();
  }
  JavaVM* jvm = nullptr;
  jni::ExceptionClearGuard guard(env);
  jint result = env->GetJavaVM(&jvm);
  (void)result;
  FIREBASE_DEV_ASSERT(result == JNI_OK);
  if (result == JNI_OK && jvm != nullptr) {
    jni::internal::GlobalRefReaper::SetJavaVM(jvm);
  }
  return jvm;
}

}  // namespace internal
}  // namespace firebase
