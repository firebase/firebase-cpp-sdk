#include "android/cancellation_token_source.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Global;
using jni::Method;
using jni::Object;

constexpr char kClassName[] =
  "com/google/android/gms/tasks/CancellationTokenSource";
Constructor<CancellationTokenSource> kConstructor("()V");
Method<Object> kGetToken("getToken",
                         "()Lcom/google/android/gms/tasks/CancellationToken;");
Method<void> kCancel("cancel", "()V");

}  // namespace

void CancellationTokenSource::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kConstructor, kGetToken, kCancel);
}

Global<CancellationTokenSource> CancellationTokenSource::Create(Env& env) {
  return env.New(kConstructor);
}

Global<Object> CancellationTokenSource::GetToken(Env& env) {
  return env.Call(*this, kGetToken);
}

void CancellationTokenSource::Cancel(Env& env) { env.Call(*this, kCancel); }

}  // namespace firestore
}  // namespace firebase
