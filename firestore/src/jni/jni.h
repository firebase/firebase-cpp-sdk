#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_JNI_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_JNI_H_

#include <jni.h>

namespace firebase {
namespace firestore {
namespace jni {

/**
 * Initializes the global `JavaVM` pointer. Should be called once per process
 * execution.
 */
void Initialize(JavaVM* vm);

/**
 * Returns the `JNIEnv` pointer for the current thread.
 */
JNIEnv* GetEnv();

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_JNI_H_
