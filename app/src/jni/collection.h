// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_APP_SRC_JNI_COLLECTION_H_
#define FIREBASE_APP_SRC_JNI_COLLECTION_H_

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/string.h"
#include "app/src/util_android.h"

namespace firebase::jni {

JNI_METHOD_LOOKUP_DECLARATION(j_string, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
                              "java/lang/String")

#define JNI_THROWABLE_METHODS(X) \
  X(GetMessage, "getMessage", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(throwable, JNI_THROWABLE_METHODS,
                              FIELD_LOOKUP_NONE, "java/lang/Throwable")

#define JNI_URI_METHODS(X) X(ToString, "toString", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(uri, JNI_URI_METHODS, FIELD_LOOKUP_NONE,
                              "android/net/Uri")

#define JNI_URI_BUILDER_METHODS(X)                                             \
  X(Constructor, "<init>", "()V")                                              \
  X(EncodedPath, "encodedPath", "(Ljava/lang/String;)Landroid/net/Uri$Builder;")\
  X(Build, "build", "()Landroid/net/Uri;")
JNI_METHOD_LOOKUP_DECLARATION(uribuilder, JNI_URI_BUILDER_METHODS,
                              FIELD_LOOKUP_NONE, "android/net/Uri$Builder")

#define JNI_ARRAY_LIST_METHODS(X)        \
  X(Constructor, "<init>", "()V")        \
  X(Add, "add", "(Ljava/lang/Object;)Z") \
  X(Size, "size", "()I")                 \
  X(Get, "get", "(I)Ljava/lang/Object;")
JNI_METHOD_LOOKUP_DECLARATION(array_list, JNI_ARRAY_LIST_METHODS,
                              FIELD_LOOKUP_NONE, "java/util/ArrayList")

#define JNI_HASH_MAP_METHODS(X)   \
  X(Constructor, "<init>", "()V") \
  X(Put, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")
JNI_METHOD_LOOKUP_DECLARATION(hash_map, JNI_HASH_MAP_METHODS, FIELD_LOOKUP_NONE,
                              "java/util/HashMap")

#define JNI_DOUBLE_METHODS(X) X(Constructor, "<init>", "(D)V")
JNI_METHOD_LOOKUP_DECLARATION(double_class, JNI_DOUBLE_METHODS,
                              FIELD_LOOKUP_NONE, "java/lang/Double")

#define JNI_BOOLEAN_METHODS(X)    \
  X(Constructor, "<init>", "(Z)V") \
  X(Value, "booleanValue", "()Z")
JNI_METHOD_LOOKUP_DECLARATION(boolean_class, JNI_BOOLEAN_METHODS,
                              FIELD_LOOKUP_NONE, "java/lang/Boolean")

#define JNI_SET_METHODS(X)                          \
  X(Iterator, "iterator", "()Ljava/util/Iterator;") \
  X(ToArray, "toArray", "()[Ljava/lang/Object;")
JNI_METHOD_LOOKUP_DECLARATION(set, JNI_SET_METHODS, FIELD_LOOKUP_NONE,
                              "java/util/Set")

#define JNI_ITERATOR_METHODS(X) \
  X(HasNext, "hasNext", "()Z")  \
  X(Next, "next", "()Ljava/lang/Object;")
JNI_METHOD_LOOKUP_DECLARATION(iterator, JNI_ITERATOR_METHODS, FIELD_LOOKUP_NONE,
                              "java/util/Iterator")

#define JNI_MAP_METHODS(X)                                                  \
  X(Put, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;") \
  X(Get, "get", "(Ljava/lang/Object;)Ljava/lang/Object;")                   \
  X(KeySet, "keySet", "()Ljava/util/Set;")
JNI_METHOD_LOOKUP_DECLARATION(map, JNI_MAP_METHODS, FIELD_LOOKUP_NONE,
                              "java/util/Map")

#define JNI_LONG_METHODS(X)     \
  X(Constructor, "<init>", "(J)V") \
  X(Value, "longValue", "()J")
JNI_METHOD_LOOKUP_DECLARATION(long_class, JNI_LONG_METHODS, FIELD_LOOKUP_NONE,
                              "java/lang/Long")

#define JNI_BUNDLE_METHODS(X)                                           \
  X(Constructor, "<init>", "()V")                                       \
  X(PutString, "putString", "(Ljava/lang/String;Ljava/lang/String;)V")  \
  X(PutLong, "putLong", "(Ljava/lang/String;J)V")                       \
  X(PutDouble, "putDouble", "(Ljava/lang/String;D)V")                   \
  X(PutBundle, "putBundle", "(Ljava/lang/String;Landroid/os/Bundle;)V") \
  X(PutParcelableArray, "putParcelableArray",                           \
    "(Ljava/lang/String;[Landroid/os/Parcelable;)V")                    \
  X(PutParcelableArrayList, "putParcelableArrayList",                   \
    "(Ljava/lang/String;Ljava/util/ArrayList;)V")
JNI_METHOD_LOOKUP_DECLARATION(bundle, JNI_BUNDLE_METHODS, FIELD_LOOKUP_NONE,
                              "android/os/Bundle")

inline bool CacheCollectionMethodIds(JNIEnv* env, jobject activity) {
  return j_string::CacheMethodIds(env, activity) &&
         throwable::CacheMethodIds(env, activity) &&
         array_list::CacheMethodIds(env, activity) &&
         hash_map::CacheMethodIds(env, activity) &&
         double_class::CacheMethodIds(env, activity) &&
         boolean_class::CacheMethodIds(env, activity) &&
         map::CacheMethodIds(env, activity) &&
         set::CacheMethodIds(env, activity) &&
         iterator::CacheMethodIds(env, activity) &&
         long_class::CacheMethodIds(env, activity) &&
         bundle::CacheMethodIds(env, activity) &&
         uri::CacheMethodIds(env, activity) &&
         uribuilder::CacheMethodIds(env, activity);
}

inline void ReleaseCollectionClasses(JNIEnv* env) {
  j_string::ReleaseClass(env);
  throwable::ReleaseClass(env);
  array_list::ReleaseClass(env);
  hash_map::ReleaseClass(env);
  double_class::ReleaseClass(env);
  boolean_class::ReleaseClass(env);
  map::ReleaseClass(env);
  set::ReleaseClass(env);
  iterator::ReleaseClass(env);
  long_class::ReleaseClass(env);
  bundle::ReleaseClass(env);
  uri::ReleaseClass(env);
  uribuilder::ReleaseClass(env);
}

inline jobject StdVectorToJavaList(JNIEnv* env,
                                   const std::vector<std::string>& vec) {
  if (!env || !array_list::CacheMethodIds(env)) return nullptr;
  jobject list = env->NewObject(array_list::GetClass(),
                                array_list::GetMethodId(array_list::kConstructor));
  if (CheckAndClearException(env) || !list) return nullptr;
  for (const auto& str : vec) {
    jstring j_str = env->NewStringUTF(str.c_str());
    if (!CheckAndClearException(env) && j_str) {
      env->CallBooleanMethod(list, array_list::GetMethodId(array_list::kAdd),
                             j_str);
      (void)CheckAndClearException(env);
      env->DeleteLocalRef(j_str);
    }
  }
  return list;
}

inline void StdMapToJavaMap(JNIEnv* env, jobject* jmap,
                            const std::map<std::string, std::string>& map) {
  if (!env || !jmap || !*jmap || !hash_map::CacheMethodIds(env)) return;
  for (const auto& pair : map) {
    jstring j_key = env->NewStringUTF(pair.first.c_str());
    if (CheckAndClearException(env) || !j_key) {
      if (j_key) env->DeleteLocalRef(j_key);
      continue;
    }
    jstring j_val = env->NewStringUTF(pair.second.c_str());
    if (!CheckAndClearException(env) && j_val) {
      jobject old_val = env->CallObjectMethod(
          *jmap, hash_map::GetMethodId(hash_map::kPut), j_key, j_val);
      (void)CheckAndClearException(env);
      if (old_val) env->DeleteLocalRef(old_val);
    }
    env->DeleteLocalRef(j_key);
    if (j_val) env->DeleteLocalRef(j_val);
  }
}

inline jobject StdMapToJavaMap(JNIEnv* env,
                               const std::map<std::string, std::string>& map) {
  if (!env || !hash_map::CacheMethodIds(env)) return nullptr;
  jobject jmap = env->NewObject(hash_map::GetClass(),
                                hash_map::GetMethodId(hash_map::kConstructor));
  if (CheckAndClearException(env) || !jmap) return nullptr;
  StdMapToJavaMap(env, &jmap, map);
  return jmap;
}

inline std::string JniUriToString(JNIEnv* env, jobject uri_obj) {
  if (!env || uri_obj == nullptr || !uri::CacheMethodIds(env)) return "";
  jobject path =
      env->CallObjectMethod(uri_obj, uri::GetMethodId(uri::kToString));
  if (CheckAndClearException(env) || !path) {
    if (path) env->DeleteLocalRef(path);
    return "";
  }
  return JniStringToStringAndReleaseLocalRef(env, path);
}

inline jobject CharsToJniUri(JNIEnv* env, const char* uri_str) {
  if (!env || !uri_str || !uribuilder::CacheMethodIds(env)) return nullptr;
  jobject uri_object = nullptr;
  jobject builder = env->NewObject(
      uribuilder::GetClass(), uribuilder::GetMethodId(uribuilder::kConstructor));
  if (CheckAndClearException(env) || !builder) return nullptr;
  jstring uri_string = env->NewStringUTF(uri_str);
  if (!CheckAndClearException(env) && uri_string) {
    jobject builder_discard = env->CallObjectMethod(
        builder, uribuilder::GetMethodId(uribuilder::kEncodedPath), uri_string);
    if (!CheckAndClearException(env)) {
      uri_object = env->CallObjectMethod(
          builder, uribuilder::GetMethodId(uribuilder::kBuild));
      if (CheckAndClearException(env)) {
        if (uri_object) env->DeleteLocalRef(uri_object);
        uri_object = nullptr;
      }
    }
    if (builder_discard) env->DeleteLocalRef(builder_discard);
  }
  if (uri_string) env->DeleteLocalRef(uri_string);
  env->DeleteLocalRef(builder);
  return uri_object;
}

inline jbyteArray ByteBufferToJavaByteArray(JNIEnv* env, const uint8_t* data,
                                            size_t size) {
  if (!env || (size > 0 && data == nullptr) ||
      size > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
    return nullptr;
  }
  jbyteArray array = env->NewByteArray(static_cast<jsize>(size));
  if (CheckAndClearException(env) || !array) return nullptr;
  if (size > 0) {
    env->SetByteArrayRegion(array, 0, static_cast<jsize>(size),
                            reinterpret_cast<const jbyte*>(data));
    if (CheckAndClearException(env)) {
      env->DeleteLocalRef(array);
      return nullptr;
    }
  }
  return array;
}

}  // namespace firebase::jni

#endif  // FIREBASE_APP_SRC_JNI_COLLECTION_H_
