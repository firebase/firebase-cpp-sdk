// Copyright 2021 Google LLC

// This file contains the declarations of common internal functions that are
// needed for the android implementation of the Firebase Performance C++
// bindings to work. Their definitions live in firebase_performance.cc.
#ifndef FIREBASE_PERFORMANCE_SRC_ANDROID_PERFORMANCE_ANDROID_INTERNAL_H_
#define FIREBASE_PERFORMANCE_SRC_ANDROID_PERFORMANCE_ANDROID_INTERNAL_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"

namespace firebase {
namespace performance {
namespace internal {

// Used to setup the cache of Performance class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define PERFORMANCE_METHODS(X)                                                \
  X(SetEnabled, "setPerformanceCollectionEnabled", "(Z)V"),                   \
  X(GetEnabled, "isPerformanceCollectionEnabled", "()Z"),                     \
  X(GetInstance, "getInstance",                                               \
      "()Lcom/google/firebase/perf/FirebasePerformance;",                     \
      firebase::jni::kMethodTypeStatic),                                     \
  X(NewTrace, "newTrace",                                                     \
      "(Ljava/lang/String;)Lcom/google/firebase/perf/metrics/Trace;"),        \
  X(NewHttpMetric, "newHttpMetric",                                           \
      "(Ljava/lang/String;Ljava/lang/String;)Lcom/google/firebase/perf/metrics/HttpMetric;") // NOLINT
// clang-format on

namespace performance_jni {
  enum Method {
    kSetEnabled,
    kGetEnabled,
    kGetInstance,
    kNewTrace,
    kNewHttpMetric,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}


// Used to setup the cache of HttpMetric class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define HTTP_METRIC_METHODS(X)                                                \
  X(StartHttpMetric, "start", "()V"),                                         \
  X(StopHttpMetric, "stop", "()V"),                                           \
  X(SetAttribute, "putAttribute", "(Ljava/lang/String;Ljava/lang/String;)V"), \
  X(GetAttribute, "getAttribute", "(Ljava/lang/String;)Ljava/lang/String;"),  \
  X(RemoveAttribute, "removeAttribute", "(Ljava/lang/String;)V"),             \
  X(SetHttpResponseCode, "setHttpResponseCode", "(I)V"),                      \
  X(SetRequestPayloadSize, "setRequestPayloadSize", "(J)V"),                  \
  X(SetResponseContentType, "setResponseContentType",                         \
    "(Ljava/lang/String;)V"),                                                 \
  X(SetResponsePayloadSize, "setResponsePayloadSize", "(J)V")
// clang-format on

namespace http_metric_jni {
  enum Method {
    kStartHttpMetric,
    kStopHttpMetric,
    kSetAttribute,
    kGetAttribute,
    kRemoveAttribute,
    kSetHttpResponseCode,
    kSetRequestPayloadSize,
    kSetResponseContentType,
    kSetResponsePayloadSize,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}


// Used to setup the cache of HttpMetric class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define TRACE_METHODS(X)                                                      \
  X(StartTrace, "start", "()V"),                                              \
  X(StopTrace, "stop", "()V"),                                                \
  X(SetAttribute, "putAttribute", "(Ljava/lang/String;Ljava/lang/String;)V"), \
  X(GetAttribute, "getAttribute", "(Ljava/lang/String;)Ljava/lang/String;"),  \
  X(RemoveAttribute, "removeAttribute", "(Ljava/lang/String;)V"),             \
  X(IncrementMetric, "incrementMetric", "(Ljava/lang/String;J)V"),            \
  X(GetLongMetric, "getLongMetric", "(Ljava/lang/String;)J"),                 \
  X(PutMetric, "putMetric", "(Ljava/lang/String;J)V")
// clang-format on

namespace trace_jni {
  enum Method {
    kStartTrace,
    kStopTrace,
    kSetAttribute,
    kGetAttribute,
    kRemoveAttribute,
    kIncrementMetric,
    kGetLongMetric,
    kPutMetric,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}


// Returns the reference to the singleton FirebasePerformance object that is
// created when firebase::performance::internal::Initialize() is called.
jobject GetFirebasePerformanceClassInstance();

// Returns the Firebase App default instance if initialized, nullptr otherwise.
const ::firebase::App* GetFirebaseApp();

}  // namespace internal
}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_ANDROID_PERFORMANCE_ANDROID_INTERNAL_H_
