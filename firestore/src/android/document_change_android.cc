#include "firestore/src/android/document_change_android.h"

#include "firestore/src/android/document_change_type_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;

using Type = DocumentChange::Type;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/DocumentChange";
Method<DocumentChangeTypeInternal> kType(
    "getType", "()Lcom/google/firebase/firestore/DocumentChange$Type;");
Method<Object> kDocument(
    "getDocument", "()Lcom/google/firebase/firestore/QueryDocumentSnapshot;");
Method<size_t> kOldIndex("getOldIndex", "()I");
Method<size_t> kNewIndex("getNewIndex", "()I");

}  // namespace

void DocumentChangeInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kDocument, kOldIndex, kNewIndex);
}

Type DocumentChangeInternal::type() const {
  Env env = GetEnv();
  Local<DocumentChangeTypeInternal> type = env.Call(obj_, kType);
  return type.GetType(env);
}

DocumentSnapshot DocumentChangeInternal::document() const {
  Env env = GetEnv();
  Local<Object> snapshot = env.Call(obj_, kDocument);
  return firestore_->NewDocumentSnapshot(env, snapshot);
}

std::size_t DocumentChangeInternal::old_index() const {
  Env env = GetEnv();
  return env.Call(obj_, kOldIndex);
}

std::size_t DocumentChangeInternal::new_index() const {
  Env env = GetEnv();
  return env.Call(obj_, kNewIndex);
}

}  // namespace firestore
}  // namespace firebase
