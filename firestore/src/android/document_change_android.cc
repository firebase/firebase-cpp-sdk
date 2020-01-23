#include "firestore/src/android/document_change_android.h"

#include <jni.h>

#include "app/src/util_android.h"
#include "firestore/src/android/document_change_type_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

using Type = DocumentChange::Type;

// clang-format off
#define DOCUMENT_CHANGE_METHODS(X)                                             \
  X(Type, "getType", "()Lcom/google/firebase/firestore/DocumentChange$Type;"), \
  X(Document, "getDocument",                                                   \
    "()Lcom/google/firebase/firestore/QueryDocumentSnapshot;"),                \
  X(OldIndex, "getOldIndex", "()I"),                                           \
  X(NewIndex, "getNewIndex", "()I")
// clang-format on

METHOD_LOOKUP_DECLARATION(document_change, DOCUMENT_CHANGE_METHODS)
METHOD_LOOKUP_DEFINITION(document_change,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/DocumentChange",
                         DOCUMENT_CHANGE_METHODS)

Type DocumentChangeInternal::type() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  jobject type = env->CallObjectMethod(
      obj_, document_change::GetMethodId(document_change::kType));
  Type result =
      DocumentChangeTypeInternal::JavaDocumentChangeTypeToDocumentChangeType(
          env, type);
  env->DeleteLocalRef(type);

  CheckAndClearJniExceptions(env);
  return result;
}

DocumentSnapshot DocumentChangeInternal::document() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  jobject snapshot = env->CallObjectMethod(
      obj_, document_change::GetMethodId(document_change::kDocument));
  CheckAndClearJniExceptions(env);

  DocumentSnapshot result{new DocumentSnapshotInternal{firestore_, snapshot}};
  env->DeleteLocalRef(snapshot);
  return result;
}

std::size_t DocumentChangeInternal::old_index() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  jint index = env->CallIntMethod(
      obj_, document_change::GetMethodId(document_change::kOldIndex));
  CheckAndClearJniExceptions(env);

  return static_cast<std::size_t>(index);
}

std::size_t DocumentChangeInternal::new_index() const {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  jint index = env->CallIntMethod(
      obj_, document_change::GetMethodId(document_change::kNewIndex));
  CheckAndClearJniExceptions(env);

  return static_cast<std::size_t>(index);
}

/* static */
bool DocumentChangeInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = document_change::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void DocumentChangeInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  document_change::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
