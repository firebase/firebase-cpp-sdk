#include "firestore/src/android/document_change_type_android.h"

#include "../include/firebase/firestore/document_change.h"
#include "app/src/assert.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Method;
using jni::Object;

using Type = DocumentChange::Type;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/DocumentChange$Type";
Method<int32_t> kOrdinal("ordinal", "()I");

}  // namespace

void DocumentChangeTypeInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kOrdinal);
}

Type DocumentChangeTypeInternal::GetType(Env& env) const {
  static constexpr int32_t kMinType = static_cast<int32_t>(Type::kAdded);
  static constexpr int32_t kMaxType = static_cast<int32_t>(Type::kRemoved);

  int32_t ordinal = env.Call(*this, kOrdinal);
  if (ordinal >= kMinType && ordinal <= kMaxType) {
    return static_cast<DocumentChange::Type>(ordinal);
  } else {
    FIREBASE_ASSERT_MESSAGE(false, "Unknown DocumentChange type.");
    return Type::kAdded;
  }
}

}  // namespace firestore
}  // namespace firebase
