// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_COMPARE_H_
#define FIREBASE_FIRESTORE_SRC_JNI_COMPARE_H_

#include "firestore/src/android/exception_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

template <typename T>
bool EqualityCompareJni(const T& lhs, const T& rhs) {
  Env env;
  env.SetUnhandledExceptionHandler(GlobalUnhandledExceptionHandler, nullptr);
  return Object::Equals(env, lhs.ToJava(), rhs.ToJava());
}

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_COMPARE_H_
