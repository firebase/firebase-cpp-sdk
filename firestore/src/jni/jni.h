// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_JNI_H_
#define FIREBASE_FIRESTORE_SRC_JNI_JNI_H_

#include <jni.h>

namespace firebase::firestore::jni {

void Initialize(JavaVM* vm);
JNIEnv* GetEnv();
JNIEnv* GetAttachedEnv();

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_JNI_H_
