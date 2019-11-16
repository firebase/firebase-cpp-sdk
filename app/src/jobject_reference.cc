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

#include "app/src/assert.h"
// util_android.h is included for GetThreadsafeJNIEnv()
#include "app/src/util_android.h"

namespace FIREBASE_NAMESPACE {
namespace internal {

JObjectReference::JObjectReference() : java_vm_(nullptr), object_(nullptr) {}

JObjectReference::JObjectReference(JNIEnv* env)
    : java_vm_(GetJavaVM(env)), object_(nullptr) {}

JObjectReference::JObjectReference(JNIEnv* env, jobject object) {
  Initialize(GetJavaVM(env), env, object);
}

JObjectReference::JObjectReference(const JObjectReference& reference) {
  Initialize(reference.java_vm_, reference.GetJNIEnv(), reference.object());
}

#ifdef FIREBASE_USE_MOVE_OPERATORS
JObjectReference::JObjectReference(JObjectReference&& reference) noexcept {
  operator=(std::move(reference));
}
#endif  // FIREBASE_USE_MOVE_OPERATORS

JObjectReference::~JObjectReference() { Set(nullptr); }

JObjectReference& JObjectReference::operator=(
    const JObjectReference& reference) {
  Set(reference.GetJNIEnv(), reference.object_);
  return *this;
}

#ifdef FIREBASE_USE_MOVE_OPERATORS
JObjectReference& JObjectReference::operator=(JObjectReference&& reference)
    noexcept {
  java_vm_ = reference.java_vm_;
  object_ = reference.object_;
  reference.java_vm_ = nullptr;
  reference.object_ = nullptr;
  return *this;
}
#endif  // FIREBASE_USE_MOVE_OPERATORS

void JObjectReference::Set(jobject jobject_reference) {
  JNIEnv* env = GetJNIEnv();
  if (env && object_) env->DeleteGlobalRef(object_);
  object_ = nullptr;
  Initialize(java_vm_, env, jobject_reference);
}

void JObjectReference::Set(JNIEnv* env, jobject jobject_reference) {
  if (env && object_) env->DeleteGlobalRef(object_);
  object_ = nullptr;
  Initialize(GetJavaVM(env), env, jobject_reference);
}

JNIEnv* JObjectReference::GetJNIEnv() const {
  return java_vm_ ? util::GetThreadsafeJNIEnv(java_vm_) : nullptr;
}

jobject JObjectReference::GetLocalRef() const {
  JNIEnv* env = GetJNIEnv();
  return object_ && env ? env->NewLocalRef(object_) : nullptr;
}

JObjectReference JObjectReference::FromLocalReference(JNIEnv* env,
                                                      jobject local_reference) {
  JObjectReference jobject_reference = JObjectReference(env, local_reference);
  if (local_reference) env->DeleteLocalRef(local_reference);
  return jobject_reference;
}

void JObjectReference::Initialize(JavaVM* jvm, JNIEnv* env,
                                  jobject jobject_reference) {
  FIREBASE_DEV_ASSERT(env || !jobject_reference);
  java_vm_ = jvm;
  object_ = nullptr;
  if (jobject_reference) object_ = env->NewGlobalRef(jobject_reference);
}

JavaVM* JObjectReference::GetJavaVM(JNIEnv* env) {
  FIREBASE_DEV_ASSERT(env);
  JavaVM* jvm = nullptr;
  jint result = env->GetJavaVM(&jvm);
  FIREBASE_DEV_ASSERT(result == JNI_OK);
  return jvm;
}

}  // namespace internal
}  // namespace FIREBASE_NAMESPACE
