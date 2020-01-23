#include "firestore/src/android/blob_android.h"

#include "app/src/util_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define BLOB_METHODS(X)                                                        \
  X(Constructor, "<init>", "(Lcom/google/protobuf/ByteString;)V",              \
    util::kMethodTypeInstance),                                                \
  X(FromBytes, "fromBytes", "([B)Lcom/google/firebase/firestore/Blob;",        \
    util::kMethodTypeStatic),                                                  \
  X(ToBytes, "toBytes", "()[B")
// clang-format on

METHOD_LOOKUP_DECLARATION(blob, BLOB_METHODS)
METHOD_LOOKUP_DEFINITION(blob,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/Blob",
                         BLOB_METHODS)

/* static */
jobject BlobInternal::BlobToJavaBlob(JNIEnv* env, const uint8_t* value,
                                     size_t size) {
  jobject byte_array = util::ByteBufferToJavaByteArray(env, value, size);
  jobject result = env->CallStaticObjectMethod(
      blob::GetClass(), blob::GetMethodId(blob::kFromBytes), byte_array);
  env->DeleteLocalRef(byte_array);
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
jbyteArray BlobInternal::JavaBlobToJbyteArray(JNIEnv* env, jobject obj) {
  jbyteArray result = static_cast<jbyteArray>(
      env->CallObjectMethod(obj, blob::GetMethodId(blob::kToBytes)));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
jclass BlobInternal::GetClass() { return blob::GetClass(); }

/* static */
bool BlobInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = blob::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void BlobInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  blob::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
