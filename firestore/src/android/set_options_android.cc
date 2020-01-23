#include "firestore/src/android/set_options_android.h"

#include <jni.h>

#include <utility>

#include "app/src/util_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

using Type = SetOptions::Type;

// clang-format off
#define SET_OPTIONS_METHODS(X)                                                 \
  X(Merge, "merge",                                                            \
    "()Lcom/google/firebase/firestore/SetOptions;",                            \
    firebase::util::kMethodTypeStatic),                                        \
  X(MergeFieldPaths, "mergeFieldPaths",                                        \
    "(Ljava/util/List;)Lcom/google/firebase/firestore/SetOptions;",            \
    firebase::util::kMethodTypeStatic)
#define SET_OPTIONS_FIELDS(X)                                                  \
  X(Overwrite, "OVERWRITE", "Lcom/google/firebase/firestore/SetOptions;",      \
    util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(set_options, SET_OPTIONS_METHODS, SET_OPTIONS_FIELDS)
METHOD_LOOKUP_DEFINITION(set_options,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/SetOptions",
                         SET_OPTIONS_METHODS, SET_OPTIONS_FIELDS)

/* static */
jobject SetOptionsInternal::Overwrite(JNIEnv* env) {
  jobject result = env->GetStaticObjectField(
      set_options::GetClass(),
      set_options::GetFieldId(set_options::kOverwrite));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
jobject SetOptionsInternal::Merge(JNIEnv* env) {
  jobject result = env->CallStaticObjectMethod(
      set_options::GetClass(), set_options::GetMethodId(set_options::kMerge));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
jobject SetOptionsInternal::ToJavaObject(JNIEnv* env,
                                         const SetOptions& set_options) {
  switch (set_options.type_) {
    case Type::kOverwrite:
      return Overwrite(env);
    case Type::kMergeAll:
      return Merge(env);
    case Type::kMergeSpecific:
      // Do below this switch.
      break;
    default:
      FIREBASE_ASSERT_MESSAGE(false, "Unknown SetOptions type.");
      return nullptr;
  }

  // Now we deal with options to merge specific fields.
  // Construct call arguments.
  jobject fields = env->NewObject(
      util::array_list::GetClass(),
      util::array_list::GetMethodId(util::array_list::kConstructor));
  jmethodID add_method = util::array_list::GetMethodId(util::array_list::kAdd);
  for (const FieldPath& field : set_options.fields_) {
    jobject field_converted = FieldPathConverter::ToJavaObject(env, field);
    env->CallBooleanMethod(fields, add_method, field_converted);
    CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(field_converted);
  }
  // Make the call to Android SDK.
  jobject result = env->CallStaticObjectMethod(
      set_options::GetClass(),
      set_options::GetMethodId(set_options::kMergeFieldPaths), fields);
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(fields);
  return result;
}

/* static */
bool SetOptionsInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = set_options::CacheMethodIds(env, activity) &&
                set_options::CacheFieldIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void SetOptionsInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  set_options::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
