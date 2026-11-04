
#ifndef FIREBASE_APP_SRC_JNI_COLLECTION_H_
#define FIREBASE_APP_SRC_JNI_COLLECTION_H_

#include <jni.h>
#include <map>
#include <string>
#include <vector>

#include "app/src/jni/declaration.h"
#include "app/src/jni/string.h"
#include "app/src/jni/env.h"

namespace firebase {
namespace jni {
JNI_METHOD_LOOKUP_DECLARATION(j_string, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE, "java/lang/String")


#define JNI_THROWABLE_METHODS(X) X(GetMessage, "getMessage", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(throwable, JNI_THROWABLE_METHODS, FIELD_LOOKUP_NONE, "java/lang/Throwable")



#define JNI_URI_METHODS(X) \
  X(ToString, "toString", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(uri, JNI_URI_METHODS, METHOD_LOOKUP_NONE, "android/net/Uri")

#define JNI_URI_BUILDER_METHODS(X) \
  X(Constructor, "<init>", "()V") \
  X(EncodedPath, "path", "(Ljava/lang/String;)Landroid/net/Uri$Builder;") \
  X(Build, "build", "()Landroid/net/Uri;")
JNI_METHOD_LOOKUP_DECLARATION(uribuilder, JNI_URI_BUILDER_METHODS, METHOD_LOOKUP_NONE, "android/net/Uri$Builder")

#define JNI_ARRAY_LIST_METHODS(X) \
  X(Constructor, "<init>", "()V") \
  X(Add, "add", "(Ljava/lang/Object;)Z") \
  X(Size, "size", "()I") \
  X(Get, "get", "(I)Ljava/lang/Object;")

JNI_METHOD_LOOKUP_DECLARATION(array_list, JNI_ARRAY_LIST_METHODS, METHOD_LOOKUP_NONE, "java/util/ArrayList")

#define JNI_HASH_MAP_METHODS(X) \
  X(Constructor, "<init>", "()V") \
  X(Put, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")

JNI_METHOD_LOOKUP_DECLARATION(hash_map, JNI_HASH_MAP_METHODS, METHOD_LOOKUP_NONE, "java/util/HashMap")


#define JNI_DOUBLE_METHODS(X) \
  X(Constructor, "<init>", "(D)V")
JNI_METHOD_LOOKUP_DECLARATION(double_class, JNI_DOUBLE_METHODS, METHOD_LOOKUP_NONE, "java/lang/Double")

#define JNI_BOOLEAN_METHODS(X) \
  X(Constructor, "<init>", "(Z)V") \
  X(Value, "booleanValue", "()Z")
JNI_METHOD_LOOKUP_DECLARATION(boolean_class, JNI_BOOLEAN_METHODS, METHOD_LOOKUP_NONE, "java/lang/Boolean")

#define JNI_SET_METHODS(X) \
  X(Iterator, "iterator", "()Ljava/util/Iterator;")
JNI_METHOD_LOOKUP_DECLARATION(set, JNI_SET_METHODS, METHOD_LOOKUP_NONE, "java/util/Set")

#define JNI_ITERATOR_METHODS(X) \
  X(HasNext, "hasNext", "()Z") \
  X(Next, "next", "()Ljava/lang/Object;")
JNI_METHOD_LOOKUP_DECLARATION(iterator, JNI_ITERATOR_METHODS, METHOD_LOOKUP_NONE, "java/util/Iterator")

#define JNI_MAP_METHODS(X) \
  X(Constructor, "<init>", "()V") \
  X(Put, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;") \
  X(Get, "get", "(Ljava/lang/Object;)Ljava/lang/Object;") \
  X(KeySet, "keySet", "()Ljava/util/Set;")
JNI_METHOD_LOOKUP_DECLARATION(map, JNI_MAP_METHODS, METHOD_LOOKUP_NONE, "java/util/Map")

#define JNI_LONG_METHODS(X) \
  X(Constructor, "<init>", "(J)V")

JNI_METHOD_LOOKUP_DECLARATION(long_class, JNI_LONG_METHODS, METHOD_LOOKUP_NONE, "java/lang/Long")

#define JNI_BUNDLE_METHODS(X) \
  X(Constructor, "<init>", "()V") \
  X(PutString, "putString", "(Ljava/lang/String;Ljava/lang/String;)V") \
  X(PutLong, "putLong", "(Ljava/lang/String;J)V") \
  X(PutDouble, "putDouble", "(Ljava/lang/String;D)V") \
  X(PutBundle, "putBundle", "(Ljava/lang/String;Landroid/os/Bundle;)V") \
  X(PutParcelableArray, "putParcelableArray", "(Ljava/lang/String;[Landroid/os/Parcelable;)V") \
  X(PutParcelableArrayList, "putParcelableArrayList", "(Ljava/lang/String;Ljava/util/ArrayList;)V")

JNI_METHOD_LOOKUP_DECLARATION(bundle, JNI_BUNDLE_METHODS, METHOD_LOOKUP_NONE, "android/os/Bundle")


inline bool CacheCollectionMethodIds(JNIEnv* env, jobject activity) {
  return array_list::CacheMethodIds(env, activity) &&
         hash_map::CacheMethodIds(env, activity) &&
         double_class::CacheMethodIds(env, activity) && boolean_class::CacheMethodIds(env, activity) && map::CacheMethodIds(env, activity) && set::CacheMethodIds(env, activity) && iterator::CacheMethodIds(env, activity) && long_class::CacheMethodIds(env, activity) && bundle::CacheMethodIds(env, activity) && uri::CacheMethodIds(env, activity) && uribuilder::CacheMethodIds(env, activity);
}

inline void ReleaseCollectionClasses(JNIEnv* env) {
  array_list::ReleaseClass(env);
  hash_map::ReleaseClass(env);
  set::ReleaseClass(env);
  iterator::ReleaseClass(env);
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

inline jobject StdVectorToJavaList(JNIEnv* env, const std::vector<std::string>& vec) {
  if (!array_list::CacheMethodIds(env)) return nullptr;
  jobject list = env->NewObject(array_list::GetClass(), array_list::GetMethodId(array_list::kConstructor));
  for (const auto& str : vec) {
    jstring j_str = env->NewStringUTF(str.c_str());
    env->CallBooleanMethod(list, array_list::GetMethodId(array_list::kAdd), j_str);
    env->DeleteLocalRef(j_str);
  }
  return list;
}

inline void StdMapToJavaMap(JNIEnv* env, jobject* jmap, const std::map<std::string, std::string>& map) {
  if (!hash_map::CacheMethodIds(env)) return;
  for (const auto& pair : map) {
    jstring j_key = env->NewStringUTF(pair.first.c_str());
    jstring j_val = env->NewStringUTF(pair.second.c_str());
    jobject old_val = env->CallObjectMethod(*jmap, hash_map::GetMethodId(hash_map::kPut), j_key, j_val);
    if (old_val) env->DeleteLocalRef(old_val);
    env->DeleteLocalRef(j_key);
    env->DeleteLocalRef(j_val);
  }
}


  inline std::string JniUriToString(JNIEnv* env, jobject uri_obj) {
    if (uri_obj == nullptr || !::firebase::jni::uri::CacheMethodIds(env)) return "";
    jobject path = env->CallObjectMethod(uri_obj, ::firebase::jni::uri::GetMethodId(::firebase::jni::uri::kToString));
    ::firebase::jni::CheckAndClearException(env);
    return ::firebase::jni::JStringToString(env, path);
  }

  inline jobject CharsToJniUri(JNIEnv* env, const char* uri_str) {
    if (!::firebase::jni::uribuilder::CacheMethodIds(env)) return nullptr;
    jobject uri_object = nullptr;
    jobject builder = env->NewObject(::firebase::jni::uribuilder::GetClass(), ::firebase::jni::uribuilder::GetMethodId(::firebase::jni::uribuilder::kConstructor));
    jstring uri_string = env->NewStringUTF(uri_str);
    jobject builder_discard = env->CallObjectMethod(builder, ::firebase::jni::uribuilder::GetMethodId(::firebase::jni::uribuilder::kEncodedPath), uri_string);
    if (!::firebase::jni::CheckAndClearException(env)) {
      uri_object = env->CallObjectMethod(builder, ::firebase::jni::uribuilder::GetMethodId(::firebase::jni::uribuilder::kBuild));
      env->DeleteLocalRef(builder_discard);
    }
    env->DeleteLocalRef(uri_string);
    env->DeleteLocalRef(builder);
    return uri_object;
  }

  inline jobject StdMapToJavaMap(JNIEnv* env, const std::map<std::string, std::string>& map) {
  if (!hash_map::CacheMethodIds(env)) return nullptr;
  jobject jmap = env->NewObject(hash_map::GetClass(), hash_map::GetMethodId(hash_map::kConstructor));
  StdMapToJavaMap(env, &jmap, map);
  return jmap;
}


inline jbyteArray ByteBufferToJavaByteArray(JNIEnv* env, const uint8_t* data, size_t size) {
  jbyteArray array = env->NewByteArray(size);
  if (size > 0) {
    env->SetByteArrayRegion(array, 0, size, reinterpret_cast<const jbyte*>(data));
  }
  return array;
}

}  // namespace jni
}  // namespace firebase


namespace firebase {
namespace util {
  inline jobject StdVectorToJavaList(JNIEnv* env, const std::vector<std::string>& vec) {
    return ::firebase::jni::StdVectorToJavaList(env, vec);
  }
  inline void StdMapToJavaMap(JNIEnv* env, jobject* jmap, const std::map<std::string, std::string>& map) {
    ::firebase::jni::StdMapToJavaMap(env, jmap, map);
  }
  
  inline std::string JniUriToString(JNIEnv* env, jobject uri_obj) {
    if (uri_obj == nullptr || !::firebase::jni::uri::CacheMethodIds(env)) return "";
    jobject path = env->CallObjectMethod(uri_obj, ::firebase::jni::uri::GetMethodId(::firebase::jni::uri::kToString));
    ::firebase::jni::CheckAndClearException(env);
    return ::firebase::jni::JStringToString(env, path);
  }

  inline jobject CharsToJniUri(JNIEnv* env, const char* uri_str) {
    if (!::firebase::jni::uribuilder::CacheMethodIds(env)) return nullptr;
    jobject uri_object = nullptr;
    jobject builder = env->NewObject(::firebase::jni::uribuilder::GetClass(), ::firebase::jni::uribuilder::GetMethodId(::firebase::jni::uribuilder::kConstructor));
    jstring uri_string = env->NewStringUTF(uri_str);
    jobject builder_discard = env->CallObjectMethod(builder, ::firebase::jni::uribuilder::GetMethodId(::firebase::jni::uribuilder::kEncodedPath), uri_string);
    if (!::firebase::jni::CheckAndClearException(env)) {
      uri_object = env->CallObjectMethod(builder, ::firebase::jni::uribuilder::GetMethodId(::firebase::jni::uribuilder::kBuild));
      env->DeleteLocalRef(builder_discard);
    }
    env->DeleteLocalRef(uri_string);
    env->DeleteLocalRef(builder);
    return uri_object;
  }

  inline jobject StdMapToJavaMap(JNIEnv* env, const std::map<std::string, std::string>& map) {
    return ::firebase::jni::StdMapToJavaMap(env, map);
  }

  inline void JavaSetToStdStringVector(JNIEnv* env, std::vector<std::string>* to, jobject from) {
    if (!from || !::firebase::jni::set::CacheMethodIds(env) || !::firebase::jni::iterator::CacheMethodIds(env)) return;
    jobject iter = env->CallObjectMethod(from, ::firebase::jni::set::GetMethodId(::firebase::jni::set::kIterator));
    if (::firebase::jni::CheckAndClearException(env) || !iter) return;
    while (env->CallBooleanMethod(iter, ::firebase::jni::iterator::GetMethodId(::firebase::jni::iterator::kHasNext))) {
      if (::firebase::jni::CheckAndClearException(env)) break;
      jobject elem_object = env->CallObjectMethod(iter, ::firebase::jni::iterator::GetMethodId(::firebase::jni::iterator::kNext));
      if (::firebase::jni::CheckAndClearException(env)) break;
      if (elem_object) {
        to->push_back(::firebase::jni::JStringToString(env, elem_object));
        env->DeleteLocalRef(elem_object);
      }
    }
    env->DeleteLocalRef(iter);
  }

  inline bool JBooleanToBool(JNIEnv* env, jobject obj) {
    if (!obj || !::firebase::jni::boolean_class::CacheMethodIds(env)) return false;
    bool result = env->CallBooleanMethod(obj, ::firebase::jni::boolean_class::GetMethodId(::firebase::jni::boolean_class::kValue));
    ::firebase::jni::CheckAndClearException(env);
    return result;
  }

  inline std::vector<unsigned char> JniByteArrayToVector(JNIEnv* env, jobject array) {
    std::vector<unsigned char> value;
    if (!array) return value;
    jbyteArray byte_array = static_cast<jbyteArray>(array);
    jsize byte_array_length = env->GetArrayLength(byte_array);
    if (byte_array_length > 0) {
      value.resize(byte_array_length);
      env->GetByteArrayRegion(byte_array, 0, byte_array_length, reinterpret_cast<jbyte*>(&value[0]));
    }
    env->DeleteLocalRef(array);
    return value;
  }
}
}

#endif  // FIREBASE_APP_SRC_JNI_COLLECTION_H_
