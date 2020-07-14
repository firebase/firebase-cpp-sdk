#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_JNI_FWD_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_JNI_FWD_H_

namespace firebase {
namespace firestore {
namespace jni {

/**
 * Returns the `JNIEnv` pointer for the current thread.
 */
JNIEnv* GetEnv();

// Reference types
template <typename T>
class Local;
template <typename T>
class Global;
template <typename T>
class NonOwning;

class Object;

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_JNI_FWD_H_
