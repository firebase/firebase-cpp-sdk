#include "firestore/src/android/metadata_changes_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Object;
using jni::StaticField;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/MetadataChanges";
StaticField<Object> kExclude("EXCLUDE",
                             "Lcom/google/firebase/firestore/MetadataChanges;");
StaticField<Object> kInclude("INCLUDE",
                             "Lcom/google/firebase/firestore/MetadataChanges;");

}  // namespace

void MetadataChangesInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kExclude, kInclude);
}

Local<Object> MetadataChangesInternal::Create(
    Env& env, MetadataChanges metadata_changes) {
  switch (metadata_changes) {
    case MetadataChanges::kExclude:
      return env.Get(kExclude);
    case MetadataChanges::kInclude:
      return env.Get(kInclude);
  }
}

}  // namespace firestore
}  // namespace firebase
