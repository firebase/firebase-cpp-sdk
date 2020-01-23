#include "firestore/src/android/server_timestamp_behavior_android.h"

namespace firebase {
namespace firestore {

using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

// clang-format off
#define SERVER_TIMESTAMP_BEHAVIOR_METHODS(X)                \
  X(Name, "name", "()Ljava/lang/String;")
#define SERVER_TIMESTAMP_BEHAVIOR_FIELDS(X)                 \
  X(None, "NONE",                                           \
    "Lcom/google/firebase/firestore/DocumentSnapshot$"      \
    "ServerTimestampBehavior;",                             \
    util::kFieldTypeStatic),                                \
  X(Estimate, "ESTIMATE",                                   \
    "Lcom/google/firebase/firestore/DocumentSnapshot$"      \
    "ServerTimestampBehavior;",                             \
    util::kFieldTypeStatic),                                \
  X(Previous, "PREVIOUS",                                   \
    "Lcom/google/firebase/firestore/DocumentSnapshot$"      \
    "ServerTimestampBehavior;",                             \
    util::kFieldTypeStatic),                                \
  X(Default, "DEFAULT",                                     \
    "Lcom/google/firebase/firestore/DocumentSnapshot$"      \
    "ServerTimestampBehavior;",                             \
    util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(server_timestamp_behavior,
                          SERVER_TIMESTAMP_BEHAVIOR_METHODS,
                          SERVER_TIMESTAMP_BEHAVIOR_FIELDS)
METHOD_LOOKUP_DEFINITION(server_timestamp_behavior,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/DocumentSnapshot$"
                         "ServerTimestampBehavior",
                         SERVER_TIMESTAMP_BEHAVIOR_METHODS,
                         SERVER_TIMESTAMP_BEHAVIOR_FIELDS)

std::map<ServerTimestampBehavior, jobject>*
    ServerTimestampBehaviorInternal::cpp_enum_to_java_ = nullptr;

/* static */
jobject ServerTimestampBehaviorInternal::ToJavaObject(
    JNIEnv* env, ServerTimestampBehavior stb) {
  return (*cpp_enum_to_java_)[stb];
}

/* static */
bool ServerTimestampBehaviorInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = server_timestamp_behavior::CacheMethodIds(env, activity) &&
                server_timestamp_behavior::CacheFieldIds(env, activity);
  util::CheckAndClearJniExceptions(env);

  // Cache Java enum values.
  cpp_enum_to_java_ = new std::map<ServerTimestampBehavior, jobject>();
  const auto add_enum = [env](ServerTimestampBehavior stb,
                              server_timestamp_behavior::Field field) {
    jobject value =
        env->GetStaticObjectField(server_timestamp_behavior::GetClass(),
                                  server_timestamp_behavior::GetFieldId(field));
    (*cpp_enum_to_java_)[stb] = env->NewGlobalRef(value);
    env->DeleteLocalRef(value);
    util::CheckAndClearJniExceptions(env);
  };
  add_enum(ServerTimestampBehavior::kDefault,
           server_timestamp_behavior::kDefault);
  add_enum(ServerTimestampBehavior::kNone, server_timestamp_behavior::kNone);
  add_enum(ServerTimestampBehavior::kEstimate,
           server_timestamp_behavior::kEstimate);
  add_enum(ServerTimestampBehavior::kPrevious,
           server_timestamp_behavior::kPrevious);

  return result;
}

/* static */
void ServerTimestampBehaviorInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  server_timestamp_behavior::ReleaseClass(env);
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
