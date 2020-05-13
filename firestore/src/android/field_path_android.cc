#include "firestore/src/android/field_path_android.h"

#include "app/src/util_android.h"
#include "firestore/src/android/util_android.h"

#if defined(__ANDROID__)
#include "firestore/src/android/field_path_portable.h"
#else  // defined(__ANDROID__)
#include "Firestore/core/src/model/field_path.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

// clang-format off
#define FIELD_PATH_METHODS(X)                                                  \
  X(Of, "of",                                                                  \
    "([Ljava/lang/String;)Lcom/google/firebase/firestore/FieldPath;",          \
    firebase::util::kMethodTypeStatic),                                        \
  X(DocumentId, "documentId",                                                  \
    "()Lcom/google/firebase/firestore/FieldPath;",                             \
    firebase::util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(field_path, FIELD_PATH_METHODS)
METHOD_LOOKUP_DEFINITION(field_path,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/FieldPath",
                         FIELD_PATH_METHODS)

/* static */
jobject FieldPathConverter::ToJavaObject(JNIEnv* env, const FieldPath& path) {
  FieldPath::FieldPathInternal* internal = path.internal_;
  // If the path is key (i.e. __name__).
  if (internal->IsKeyFieldPath()) {
    jobject result = env->CallStaticObjectMethod(
        field_path::GetClass(),
        field_path::GetMethodId(field_path::kDocumentId));
    CheckAndClearJniExceptions(env);
    return result;
  }

  // Prepare call arguments.
  jsize size = static_cast<jsize>(internal->size());
  jobjectArray args =
      env->NewObjectArray(size, firebase::util::string::GetClass(),
                          /*initialElement=*/nullptr);
  for (jsize i = 0; i < size; ++i) {
    jobject segment = env->NewStringUTF((*internal)[i].c_str());
    env->SetObjectArrayElement(args, i, segment);
    env->DeleteLocalRef(segment);
    CheckAndClearJniExceptions(env);
  }

  // Make JNI call and check for error.
  jobject result = env->CallStaticObjectMethod(
      field_path::GetClass(), field_path::GetMethodId(field_path::kOf), args);
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(args);

  return result;
}

/* static */
bool FieldPathConverter::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = field_path::CacheMethodIds(env, activity);
  firebase::util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void FieldPathConverter::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  field_path::ReleaseClass(env);
  firebase::util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
