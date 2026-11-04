// Copyright 2021 Google LLC

#include "firestore/src/android/load_bundle_task_progress_android.h"

#include <cstdint>

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticField;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/LoadBundleTaskProgress";
Method<int32_t> kGetDocumentsLoaded("getDocumentsLoaded", "()I");
Method<int32_t> kGetTotalDocuments("getTotalDocuments", "()I");
Method<int64_t> kGetBytesLoaded("getBytesLoaded", "()J");
Method<int64_t> kGetTotalBytes("getTotalBytes", "()J");
Method<Object> kGetTaskState(
    "getTaskState",
    "()Lcom/google/firebase/firestore/LoadBundleTaskProgress$TaskState;");

constexpr char kStateEnumName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/LoadBundleTaskProgress$TaskState";
StaticField<Object> kTaskStateSuccess(
    "SUCCESS",
    "Lcom/google/firebase/firestore/LoadBundleTaskProgress$TaskState;");
StaticField<Object> kTaskStateRunning(
    "RUNNING",
    "Lcom/google/firebase/firestore/LoadBundleTaskProgress$TaskState;");

LoadBundleTaskProgress::State ToPublicState(Env& env, const Object& progress) {
  Local<Object> state = env.Call(progress, kGetTaskState);
  if (!env.ok()) return LoadBundleTaskProgress::State::kError;
  if (Object::Equals(env, state, env.Get(kTaskStateSuccess))) {
    return LoadBundleTaskProgress::State::kSuccess;
  }
  if (Object::Equals(env, state, env.Get(kTaskStateRunning))) {
    return LoadBundleTaskProgress::State::kInProgress;
  }
  return LoadBundleTaskProgress::State::kError;
}

}  // namespace

void LoadBundleTaskProgressInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kGetDocumentsLoaded, kGetTotalDocuments,
                   kGetBytesLoaded, kGetTotalBytes, kGetTaskState);
  loader.LoadClass(kStateEnumName, kTaskStateSuccess, kTaskStateRunning);
}

LoadBundleTaskProgress LoadBundleTaskProgressInternal::ToPublic(
    Env& env, const Object& progress) {
  if (!progress) return {};
  return LoadBundleTaskProgress(env.Call(progress, kGetDocumentsLoaded),
                                env.Call(progress, kGetTotalDocuments),
                                env.Call(progress, kGetBytesLoaded),
                                env.Call(progress, kGetTotalBytes),
                                ToPublicState(env, progress));
}

int32_t LoadBundleTaskProgressInternal::documents_loaded() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetDocumentsLoaded);
}
int32_t LoadBundleTaskProgressInternal::total_documents() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetTotalDocuments);
}
int64_t LoadBundleTaskProgressInternal::bytes_loaded() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetBytesLoaded);
}
int64_t LoadBundleTaskProgressInternal::total_bytes() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetTotalBytes);
}
LoadBundleTaskProgress::State LoadBundleTaskProgressInternal::state() const {
  Env env = GetEnv();
  return ToPublicState(env, obj_.get(env));
}

}  // namespace firebase::firestore
