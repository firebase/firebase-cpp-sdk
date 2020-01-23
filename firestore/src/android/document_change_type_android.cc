#include "firestore/src/android/document_change_type_android.h"

#include "app/src/util_android.h"

namespace firebase {
namespace firestore {

using Type = DocumentChange::Type;

// clang-format off
#define DOCUMENT_CHANGE_TYPE_METHODS(X) X(Name, "name", "()Ljava/lang/String;")
#define DOCUMENT_CHANGE_TYPE_FIELDS(X)                                         \
  X(Added, "ADDED", "Lcom/google/firebase/firestore/DocumentChange$Type;",     \
    util::kFieldTypeStatic),                                                   \
  X(Modified, "MODIFIED",                                                      \
    "Lcom/google/firebase/firestore/DocumentChange$Type;",                     \
    util::kFieldTypeStatic),                                                   \
  X(Removed, "REMOVED", "Lcom/google/firebase/firestore/DocumentChange$Type;", \
    util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(document_change_type, DOCUMENT_CHANGE_TYPE_METHODS,
                          DOCUMENT_CHANGE_TYPE_FIELDS)
METHOD_LOOKUP_DEFINITION(document_change_type,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/DocumentChange$Type",
                         DOCUMENT_CHANGE_TYPE_METHODS,
                         DOCUMENT_CHANGE_TYPE_FIELDS)

std::map<Type, jobject>* DocumentChangeTypeInternal::cpp_enum_to_java_ =
    nullptr;

/* static */
Type DocumentChangeTypeInternal::JavaDocumentChangeTypeToDocumentChangeType(
    JNIEnv* env, jobject type) {
  for (const auto& kv : *cpp_enum_to_java_) {
    if (env->IsSameObject(type, kv.second)) {
      return kv.first;
    }
  }
  FIREBASE_ASSERT_MESSAGE(false, "Unknown DocumentChange type.");
  return Type::kAdded;
}

/* static */
bool DocumentChangeTypeInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = document_change_type::CacheMethodIds(env, activity) &&
                document_change_type::CacheFieldIds(env, activity);
  util::CheckAndClearJniExceptions(env);

  // Cache Java enum values.
  cpp_enum_to_java_ = new std::map<Type, jobject>();
  const auto add_enum = [env](Type type, document_change_type::Field field) {
    jobject value =
        env->GetStaticObjectField(document_change_type::GetClass(),
                                  document_change_type::GetFieldId(field));
    (*cpp_enum_to_java_)[type] = env->NewGlobalRef(value);
    env->DeleteLocalRef(value);
    util::CheckAndClearJniExceptions(env);
  };
  add_enum(Type::kAdded, document_change_type::kAdded);
  add_enum(Type::kModified, document_change_type::kModified);
  add_enum(Type::kRemoved, document_change_type::kRemoved);

  return result;
}

/* static */
void DocumentChangeTypeInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  document_change_type::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);

  // Uncache Java enum values.
  for (auto& kv : *cpp_enum_to_java_) {
    env->DeleteGlobalRef(kv.second);
  }
  util::CheckAndClearJniExceptions(env);
  delete cpp_enum_to_java_;
  cpp_enum_to_java_ = nullptr;
}

}  // namespace firestore
}  // namespace firebase
