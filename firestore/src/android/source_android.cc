#include "firestore/src/android/source_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define SOURCE_METHODS(X)                \
  X(Name, "name", "()Ljava/lang/String;")
#define SOURCE_FIELDS(X)                                          \
  X(Default, "DEFAULT", "Lcom/google/firebase/firestore/Source;", \
    util::kFieldTypeStatic),                                      \
  X(Server, "SERVER", "Lcom/google/firebase/firestore/Source;",   \
    util::kFieldTypeStatic),                                      \
  X(Cache, "CACHE", "Lcom/google/firebase/firestore/Source;",     \
    util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(source, SOURCE_METHODS, SOURCE_FIELDS)
METHOD_LOOKUP_DEFINITION(source,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/Source",
                         SOURCE_METHODS, SOURCE_FIELDS)

std::map<Source, jobject>* SourceInternal::cpp_enum_to_java_ = nullptr;

/* static */
jobject SourceInternal::ToJavaObject(JNIEnv* env, Source source) {
  return (*cpp_enum_to_java_)[source];
}

/* static */
bool SourceInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = source::CacheMethodIds(env, activity) &&
                source::CacheFieldIds(env, activity);
  util::CheckAndClearJniExceptions(env);

  // Cache Java enum values.
  cpp_enum_to_java_ = new std::map<Source, jobject>();
  const auto add_enum = [env](Source source, source::Field field) {
    jobject value = env->GetStaticObjectField(source::GetClass(),
                                              source::GetFieldId(field));
    (*cpp_enum_to_java_)[source] = env->NewGlobalRef(value);
    env->DeleteLocalRef(value);
    util::CheckAndClearJniExceptions(env);
  };
  add_enum(Source::kDefault, source::kDefault);
  add_enum(Source::kServer, source::kServer);
  add_enum(Source::kCache, source::kCache);

  return result;
}

/* static */
void SourceInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  source::ReleaseClass(env);
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
