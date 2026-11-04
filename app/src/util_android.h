// Copyright 2016 Google LLC
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

#ifndef FIREBASE_APP_SRC_UTIL_ANDROID_H_
#define FIREBASE_APP_SRC_UTIL_ANDROID_H_

#include <jni.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/global.h"
#include "app/src/jni/local.h"
#include "app/src/jni/ownership.h"
#include "app/src/jni/string.h"
#include "app/src/jni/traits.h"
#include "app/src/log.h"

#define PROGUARD_KEEP_CLASS "%PG%"

namespace firebase {
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
typedef struct {
  const char* name;
  const char* signature;
  void* fnPtr;  // NOLINT
} JNINativeMethod;

inline const ::JNINativeMethod* ConvertJNINativeMethod(
    const JNINativeMethod* source, size_t number_of_native_methods) {
  if (!source || number_of_native_methods == 0) return nullptr;
  ::JNINativeMethod* target = new ::JNINativeMethod[number_of_native_methods];
  for (size_t i = 0; i < number_of_native_methods; ++i) {
    target[i].name = const_cast<char*>(source[i].name);
    target[i].signature = const_cast<char*>(source[i].signature);
    target[i].fnPtr = source[i].fnPtr;
  }
  return target;
}
inline void CleanUpConvertedJNINativeMethod(const ::JNINativeMethod* target) {
  delete[] const_cast<::JNINativeMethod*>(target);
}
#else
typedef ::JNINativeMethod JNINativeMethod;
inline const ::JNINativeMethod* ConvertJNINativeMethod(
    const JNINativeMethod* source, size_t number_of_native_methods) {
  (void)number_of_native_methods;
  return source;
}
inline void CleanUpConvertedJNINativeMethod(const ::JNINativeMethod* target) {
  (void)target;
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

namespace util {

enum MethodType { kMethodTypeInstance, kMethodTypeStatic };
enum MethodRequirement { kMethodRequired, kMethodOptional };

struct MethodNameSignature {
  const char* name;
  const char* signature;
  const MethodType type;
  const MethodRequirement optional;
};

enum FieldType { kFieldTypeInstance, kFieldTypeStatic };

struct FieldDescriptor {
  const char* name;
  const char* signature;
  const FieldType type;
  const MethodRequirement optional;
};

#ifndef FIREBASE_UTIL_CLASS_REQUIREMENT_DEFINED
#define FIREBASE_UTIL_CLASS_REQUIREMENT_DEFINED
enum ClassRequirement { kClassRequired, kClassOptional };
#endif

#define RESOURCE_TYPES(X) X(String, "string"), X(Id, "id")
#define RESOURCE_TYPE_ENUM(type_identifier, type_name) \
  kResourceType##type_identifier
#define RESOURCE_TYPE_STRING(type_identifier, type_name) type_name

enum ResourceType { RESOURCE_TYPES(RESOURCE_TYPE_ENUM) };

bool Initialize(JNIEnv* env, jobject activity_object);
bool InitializeActivityClasses(JNIEnv* env, jobject activity_object);
void Terminate(JNIEnv* env);
void TerminateActivityClasses(JNIEnv* env);

bool LookupMethodIds(JNIEnv* env, jclass clazz,
                     const MethodNameSignature* method_name_signatures,
                     size_t number_of_method_name_signatures,
                     jmethodID* method_ids, const char* class_name);
bool LookupFieldIds(JNIEnv* env, jclass clazz,
                    const FieldDescriptor* field_descriptors,
                    size_t number_of_field_descriptors, jfieldID* field_ids,
                    const char* class_name);

#define METHOD_NAME_INFO_EXPANDER(arg0, arg1, arg2, arg3, arg4, arg5, \
                                  function, ...)                      \
  function
#define METHOD_NAME_SIGNATURE_5(id, name, signature, method_type, optional) \
  { name, signature, method_type, optional }
#define METHOD_NAME_SIGNATURE_4(id, name, signature, method_type) \
  METHOD_NAME_SIGNATURE_5(                                        \
      id, name, signature, method_type,                           \
      ::firebase::util::MethodRequirement::kMethodRequired)
#define METHOD_NAME_SIGNATURE_3(id, name, signature) \
  METHOD_NAME_SIGNATURE_4(id, name, signature,       \
                          ::firebase::util::kMethodTypeInstance)
#define METHOD_NAME_SIGNATURE(...)                                        \
  METHOD_NAME_INFO_EXPANDER(, ##__VA_ARGS__,                              \
                            METHOD_NAME_SIGNATURE_5(__VA_ARGS__),         \
                            METHOD_NAME_SIGNATURE_4(__VA_ARGS__),         \
                            METHOD_NAME_SIGNATURE_3(__VA_ARGS__),         \
                            numargs2(__VA_ARGS__), numargs1(__VA_ARGS__), )

#define METHOD_ID_3(id, name, signature) k##id
#define METHOD_ID_4(id, name, signature, method_type) k##id
#define METHOD_ID_5(id, name, signature, method_type, optional) k##id
#define METHOD_ID(...)                                                        \
  METHOD_NAME_INFO_EXPANDER(                                                  \
      , ##__VA_ARGS__, METHOD_ID_5(__VA_ARGS__), METHOD_ID_4(__VA_ARGS__),    \
      METHOD_ID_3(__VA_ARGS__), numargs2(__VA_ARGS__), numargs1(__VA_ARGS__), )

#define FIELD_DESCRIPTOR(...) METHOD_NAME_SIGNATURE(__VA_ARGS__)
#define FIELD_ID(...) METHOD_ID(__VA_ARGS__)

#undef METHOD_LOOKUP_NONE
#define METHOD_LOOKUP_NONE(X)                        \
  X(InvalidMethod, nullptr, nullptr,                 \
    ::firebase::util::MethodType::kMethodTypeStatic, \
    ::firebase::util::MethodRequirement::kMethodOptional)

#undef FIELD_LOOKUP_NONE
#define FIELD_LOOKUP_NONE(X)                       \
  X(InvalidField, nullptr, nullptr,                \
    ::firebase::util::FieldType::kFieldTypeStatic, \
    ::firebase::util::MethodRequirement::kMethodOptional)

#define METHOD_LOOKUP_DECLARATION_3(namespace_identifier,                      \
                                    method_descriptor_macro,                   \
                                    field_descriptor_macro)                    \
  namespace namespace_identifier {                                             \
  enum Method { method_descriptor_macro(METHOD_ID), kMethodCount };            \
  enum Field { field_descriptor_macro(FIELD_ID), kFieldCount };                \
  jclass CacheClassFromFiles(                                                  \
      JNIEnv* env, jobject activity_object,                                    \
      const std::vector<::firebase::internal::EmbeddedFile>* embedded_files,   \
      ::firebase::util::ClassRequirement optional);                            \
  jclass CacheClassFromFiles(                                                  \
      JNIEnv* env, jobject activity_object,                                    \
      const std::vector<::firebase::internal::EmbeddedFile>* embedded_files);  \
  jclass CacheClass(JNIEnv* env, jobject activity_object,                      \
                    ::firebase::util::ClassRequirement optional);              \
  jclass CacheClass(JNIEnv* env, jobject activity_object);                     \
  jclass CacheClass(JNIEnv* env);                                              \
  jclass GetClass();                                                           \
  bool RegisterNatives(JNIEnv* env, const JNINativeMethod* native_methods,     \
                       size_t number_of_native_methods);                       \
  void ReleaseClass(JNIEnv* env);                                              \
  bool CacheMethodIds(JNIEnv* env, jobject activity_object);                   \
  jmethodID GetMethodId(Method method);                                        \
  bool CacheFieldIds(JNIEnv* env, jobject activity_object);                    \
  jfieldID GetFieldId(Field field);                                            \
  }  // namespace namespace_identifier

#define METHOD_LOOKUP_DECLARATION_2(namespace_identifier, \
                                    method_descriptor_macro) \
  METHOD_LOOKUP_DECLARATION_3(namespace_identifier, method_descriptor_macro, \
                              FIELD_LOOKUP_NONE)
#define METHOD_LOOKUP_DECLARATION_EXPANDER(arg0, arg1, arg2, arg3, \
                                           function, ...)          \
  function
#define METHOD_LOOKUP_DECLARATION(...)                                   \
  METHOD_LOOKUP_DECLARATION_EXPANDER(                                    \
      , ##__VA_ARGS__, METHOD_LOOKUP_DECLARATION_3(__VA_ARGS__),         \
      METHOD_LOOKUP_DECLARATION_2(__VA_ARGS__), numargs1(__VA_ARGS__),   \
      numargs0(__VA_ARGS__))

#define METHOD_LOOKUP_DEFINITION_4(namespace_identifier, class_name,          \
                                   method_descriptor_macro,                   \
                                   field_descriptor_macro)                    \
  namespace namespace_identifier {                                            \
  static const char* kClassName =                                             \
      (class_name && class_name[0] == '%' && class_name[1] == 'P' &&          \
       class_name[2] == 'G' && class_name[3] == '%')                          \
          ? &class_name[4]                                                    \
          : class_name;                                                       \
  static const ::firebase::util::MethodNameSignature kMethodSignatures[] = {  \
      method_descriptor_macro(METHOD_NAME_SIGNATURE),                         \
  };                                                                          \
  static const ::firebase::util::FieldDescriptor kFieldDescriptors[] = {      \
      field_descriptor_macro(FIELD_DESCRIPTOR),                               \
  };                                                                          \
  static jmethodID g_method_ids[kMethodCount];                                \
  static jfieldID g_field_ids[kFieldCount];                                   \
  static jclass g_class = nullptr;                                            \
  static bool g_registered_natives = false;                                   \
  static ::firebase::Mutex g_mutex;                                           \
  jclass CacheClassFromFiles(                                                 \
      JNIEnv* env, jobject activity_object,                                   \
      const std::vector<::firebase::internal::EmbeddedFile>* embedded_files,  \
      ::firebase::util::ClassRequirement optional) {                          \
    ::firebase::MutexLock lock(g_mutex);                                      \
    if (!g_class) {                                                           \
      g_class = ::firebase::util::FindClassGlobal(                            \
          env, activity_object, embedded_files, kClassName, optional);        \
    }                                                                         \
    return g_class;                                                           \
  }                                                                           \
  jclass CacheClassFromFiles(                                                 \
      JNIEnv* env, jobject activity_object,                                   \
      const std::vector<::firebase::internal::EmbeddedFile>* embedded_files) {\
    return CacheClassFromFiles(                                               \
        env, activity_object, embedded_files,                                 \
        ::firebase::util::ClassRequirement::kClassRequired);                  \
  }                                                                           \
  jclass CacheClass(JNIEnv* env, jobject activity_object,                     \
                    ::firebase::util::ClassRequirement optional) {            \
    return CacheClassFromFiles(env, activity_object, nullptr, optional);      \
  }                                                                           \
  jclass CacheClass(JNIEnv* env, jobject activity_object) {                   \
    return CacheClassFromFiles(                                               \
        env, activity_object, nullptr,                                        \
        ::firebase::util::ClassRequirement::kClassRequired);                  \
  }                                                                           \
  jclass CacheClass(JNIEnv* env) { return CacheClass(env, nullptr); }         \
  jclass GetClass() { return g_class; }                                       \
  bool RegisterNatives(JNIEnv* env, const JNINativeMethod* native_methods,    \
                       size_t number_of_native_methods) {                     \
    ::firebase::MutexLock lock(g_mutex);                                      \
    if (g_registered_natives || !env || !g_class || !native_methods ||        \
        number_of_native_methods == 0 ||                                      \
        number_of_native_methods > static_cast<size_t>(INT32_MAX)) {          \
      return false;                                                           \
    }                                                                         \
    ::firebase::util::CheckAndClearJniExceptions(env);                        \
    const ::JNINativeMethod* true_native_methods =                            \
        ::firebase::ConvertJNINativeMethod(native_methods,                    \
                                           number_of_native_methods);         \
    const jint register_status = env->RegisterNatives(                        \
        GetClass(), true_native_methods,                                      \
        static_cast<jint>(number_of_native_methods));                         \
    ::firebase::CleanUpConvertedJNINativeMethod(true_native_methods);         \
    const bool had_exception =                                                \
        ::firebase::util::CheckAndClearJniExceptions(env);                    \
    g_registered_natives = (register_status == JNI_OK && !had_exception);     \
    return g_registered_natives;                                              \
  }                                                                           \
  void ReleaseClass(JNIEnv* env) {                                            \
    ::firebase::MutexLock lock(g_mutex);                                      \
    for (size_t i = 0; i < static_cast<size_t>(kMethodCount); ++i)            \
      g_method_ids[i] = nullptr;                                              \
    for (size_t i = 0; i < static_cast<size_t>(kFieldCount); ++i)             \
      g_field_ids[i] = nullptr;                                               \
    if (g_class) {                                                            \
      if (env) {                                                              \
        ::firebase::util::CheckAndClearJniExceptions(env);                    \
        if (g_registered_natives) {                                           \
          env->UnregisterNatives(g_class);                                    \
          ::firebase::util::CheckAndClearJniExceptions(env);                  \
        }                                                                     \
      }                                                                       \
      ::firebase::jni::internal::GlobalRefReaper::DeleteOrEnqueue(g_class,    \
                                                                  env);       \
      g_class = nullptr;                                                      \
    }                                                                         \
    g_registered_natives = false;                                             \
  }                                                                           \
  bool CacheMethodIds(JNIEnv* env, jobject activity_object) {                 \
    return ::firebase::util::LookupMethodIds(                                 \
        env, CacheClass(env, activity_object), kMethodSignatures,             \
        kMethodCount, g_method_ids, kClassName);                              \
  }                                                                           \
  jmethodID GetMethodId(Method method) {                                      \
    FIREBASE_ASSERT(static_cast<int>(method) >= 0 && method < kMethodCount);  \
    return g_method_ids[method];                                              \
  }                                                                           \
  bool CacheFieldIds(JNIEnv* env, jobject activity_object) {                  \
    return ::firebase::util::LookupFieldIds(                                  \
        env, CacheClass(env, activity_object), kFieldDescriptors, kFieldCount,\
        g_field_ids, kClassName);                                             \
  }                                                                           \
  jfieldID GetFieldId(Field field) {                                          \
    FIREBASE_ASSERT(static_cast<int>(field) >= 0 && field < kFieldCount);     \
    return g_field_ids[field];                                                \
  }                                                                           \
  }  // namespace namespace_identifier

#define METHOD_LOOKUP_DEFINITION_3(namespace_identifier, class_name, \
                                   method_descriptor_macro)          \
  METHOD_LOOKUP_DEFINITION_4(namespace_identifier, class_name,       \
                             method_descriptor_macro, FIELD_LOOKUP_NONE)
#define METHOD_LOOKUP_DEFINITION_EXPANDER(arg0, arg1, arg2, arg3, arg4, \
                                          function, ...)                \
  function
#define METHOD_LOOKUP_DEFINITION(...)                                 \
  METHOD_LOOKUP_DEFINITION_EXPANDER(                                  \
      , ##__VA_ARGS__, METHOD_LOOKUP_DEFINITION_4(__VA_ARGS__),       \
      METHOD_LOOKUP_DEFINITION_3(__VA_ARGS__), numargs2(__VA_ARGS__), \
      numargs1(__VA_ARGS__), numargs0(__VA_ARGS__))

#define ACTIVITY_BASE_METHODS(X)                                        \
  X(GetApplicationContext, "getApplicationContext",                     \
    "()Landroid/content/Context;"),                                     \
  X(GetCacheDir, "getCacheDir", "()Ljava/io/File;"),                    \
  X(GetClassLoader, "getClassLoader", "()Ljava/lang/ClassLoader;"),     \
  X(GetIntent, "getIntent", "()Landroid/content/Intent;"),              \
  X(GetPackageName, "getPackageName", "()Ljava/lang/String;"),          \
  X(GetResources, "getResources", "()Landroid/content/res/Resources;"), \
  X(Finish, "finish", "()V"),                                           \
  X(GetContentResolver, "getContentResolver",                           \
    "()Landroid/content/ContentResolver;"),                             \
  X(GetString, "getString", "(I)Ljava/lang/String;")
#if !defined(FIREBASE_ANDROID_FOR_DESKTOP)
#define ACTIVITY_METHODS(X)                                          \
  ACTIVITY_BASE_METHODS(X),                                          \
  X(GetCodeCacheDir, "getCodeCacheDir", "()Ljava/io/File;",          \
    ::firebase::util::kMethodTypeInstance, ::firebase::util::kMethodOptional)
#else
#define ACTIVITY_METHODS(X) ACTIVITY_BASE_METHODS(X)
#endif
METHOD_LOOKUP_DECLARATION(activity, ACTIVITY_METHODS)

#define BUNDLE_METHODS(X)                                                  \
  X(Constructor, "<init>", "()V"),                                         \
  X(GetString, "getString", "(Ljava/lang/String;)Ljava/lang/String;"),     \
  X(KeySet, "keySet", "()Ljava/util/Set;"),                                \
  X(PutFloat, "putFloat", "(Ljava/lang/String;F)V"),                       \
  X(PutDouble, "putDouble", "(Ljava/lang/String;D)V"),                     \
  X(PutLong, "putLong", "(Ljava/lang/String;J)V"),                         \
  X(PutString, "putString", "(Ljava/lang/String;Ljava/lang/String;)V"),    \
  X(PutBundle, "putBundle", "(Ljava/lang/String;Landroid/os/Bundle;)V"),   \
  X(PutParcelableArrayList, "putParcelableArrayList",                      \
    "(Ljava/lang/String;Ljava/util/ArrayList;)V")
METHOD_LOOKUP_DECLARATION(bundle, BUNDLE_METHODS)

#define CLASS_METHODS(X) \
  X(IsArray, "isArray", "()Z"), X(GetName, "getName", "()Ljava/lang/String;")
METHOD_LOOKUP_DECLARATION(class_class, CLASS_METHODS)

#define ITERATOR_METHODS(X) \
  X(HasNext, "hasNext", "()Z"), X(Next, "next", "()Ljava/lang/Object;")
METHOD_LOOKUP_DECLARATION(iterator, ITERATOR_METHODS)

#define ITERABLE_METHODS(X) X(Iterator, "iterator", "()Ljava/util/Iterator;")
METHOD_LOOKUP_DECLARATION(iterable, ITERABLE_METHODS)

#define SET_METHODS(X)                             \
  X(Iterator, "iterator", "()Ljava/util/Iterator;"), \
  X(ToArray, "toArray", "()[Ljava/lang/Object;")
METHOD_LOOKUP_DECLARATION(set, SET_METHODS)

#define LIST_METHODS(X)                                       \
  X(Get, "get", "(I)Ljava/lang/Object;"),                     \
  X(Set, "set", "(ILjava/lang/Object;)Ljava/lang/Object;"),   \
  X(Size, "size", "()I")
METHOD_LOOKUP_DECLARATION(list, LIST_METHODS)

#define ARRAY_LIST_METHODS(X)               \
  X(Constructor, "<init>", "()V"),          \
  X(ConstructorWithSize, "<init>", "(I)V"), \
  X(Add, "add", "(Ljava/lang/Object;)Z")
METHOD_LOOKUP_DECLARATION(array_list, ARRAY_LIST_METHODS)

#define INTENT_METHODS(X)                                                \
  X(Constructor, "<init>", "(Landroid/content/Context;Ljava/lang/Class;)V"), \
  X(PutExtraString, "putExtra",                                          \
    "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;"),   \
  X(GetExtras, "getExtras", "()Landroid/os/Bundle;"),                    \
  X(GetIntExtra, "getIntExtra", "(Ljava/lang/String;I)I"),               \
  X(GetData, "getData", "()Landroid/net/Uri;")
METHOD_LOOKUP_DECLARATION(intent, INTENT_METHODS)

#define FILE_METHODS(X)                                                    \
  X(ConstructorFilePath, "<init>", "(Ljava/io/File;Ljava/lang/String;)V"), \
  X(GetAbsolutePath, "getAbsolutePath", "()Ljava/lang/String;"),           \
  X(GetPath, "getPath", "()Ljava/lang/String;"),                           \
  X(Exists, "exists", "()Z"),                                              \
  X(Delete, "delete", "()Z"),                                              \
  X(SetReadOnly, "setReadOnly", "()Z"),                                    \
  X(SetWritable, "setWritable", "(Z)Z"),                                   \
  X(ToUri, "toURI", "()Ljava/net/URI;")
METHOD_LOOKUP_DECLARATION(file, FILE_METHODS)

#define CONTEXT_METHODS(X)                                          \
  X(GetFilesDir, "getFilesDir", "()Ljava/io/File;"),                \
  X(StartService, "startService",                                   \
    "(Landroid/content/Intent;)Landroid/content/ComponentName;"),   \
  X(GetPackageName, "getPackageName", "()Ljava/lang/String;")
METHOD_LOOKUP_DECLARATION(context, CONTEXT_METHODS)

#define DATE_METHODS(X)                       \
  X(Constructor, "<init>", "()V"),            \
  X(ConstructorWithTime, "<init>", "(J)V"),   \
  X(GetTime, "getTime", "()J")
METHOD_LOOKUP_DECLARATION(date, DATE_METHODS)

#define MAP_METHODS(X)                                                       \
  X(Put, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"), \
  X(Get, "get", "(Ljava/lang/Object;)Ljava/lang/Object;"),                   \
  X(KeySet, "keySet", "()Ljava/util/Set;")
METHOD_LOOKUP_DECLARATION(map, MAP_METHODS)

#define HASHMAP_METHODS(X) X(Constructor, "<init>", "()V")
METHOD_LOOKUP_DECLARATION(hash_map, HASHMAP_METHODS)

#define DOUBLE_METHODS(X) \
  X(Constructor, "<init>", "(D)V"), X(Value, "doubleValue", "()D")
METHOD_LOOKUP_DECLARATION(double_class, DOUBLE_METHODS)

#define ENUM_METHODS(X) \
  X(Equals, "equals", "(Ljava/lang/Object;)Z"), \
  X(Name, "name", "()Ljava/lang/String;")
METHOD_LOOKUP_DECLARATION(enum_class, ENUM_METHODS)

#define BOOLEAN_METHODS(X) \
  X(Constructor, "<init>", "(Z)V"), X(Value, "booleanValue", "()Z")
METHOD_LOOKUP_DECLARATION(boolean_class, BOOLEAN_METHODS)

#define LONG_METHODS(X) \
  X(Constructor, "<init>", "(J)V"), X(Value, "longValue", "()J")
METHOD_LOOKUP_DECLARATION(long_class, LONG_METHODS)

#define BYTE_METHODS(X) \
  X(Constructor, "<init>", "(B)V"), X(Value, "byteValue", "()B")
METHOD_LOOKUP_DECLARATION(byte_class, BYTE_METHODS)

#define CHARACTER_METHODS(X) \
  X(Constructor, "<init>", "(C)V"), X(Value, "charValue", "()C")
METHOD_LOOKUP_DECLARATION(character_class, CHARACTER_METHODS)

#define FLOAT_METHODS(X) \
  X(Constructor, "<init>", "(F)V"), X(Value, "floatValue", "()F")
METHOD_LOOKUP_DECLARATION(float_class, FLOAT_METHODS)

#define INTEGER_METHODS(X) \
  X(Constructor, "<init>", "(I)V"), X(Value, "intValue", "()I")
METHOD_LOOKUP_DECLARATION(integer_class, INTEGER_METHODS)

#define SHORT_METHODS(X) \
  X(Constructor, "<init>", "(S)V"), X(Value, "shortValue", "()S")
METHOD_LOOKUP_DECLARATION(short_class, SHORT_METHODS)

#define STRING_METHODS(X) X(Constructor, "<init>", "()V")
METHOD_LOOKUP_DECLARATION(string, STRING_METHODS)

#define THROWABLE_METHODS(X)                                               \
  X(GetLocalizedMessage, "getLocalizedMessage", "()Ljava/lang/String;"),   \
  X(GetMessage, "getMessage", "()Ljava/lang/String;"),                     \
  X(ToString, "toString", "()Ljava/lang/String;")
METHOD_LOOKUP_DECLARATION(throwable, THROWABLE_METHODS)

#define URI_METHODS(X)                                            \
  X(ToString, "toString", "()Ljava/lang/String;"),                \
  X(GetPath, "getPath", "()Ljava/lang/String;"),                  \
  X(Parse, "parse", "(Ljava/lang/String;)Landroid/net/Uri;",      \
    ::firebase::util::kMethodTypeStatic)
METHOD_LOOKUP_DECLARATION(uri, URI_METHODS)

#define OBJECT_METHODS(X) X(ToString, "toString", "()Ljava/lang/String;")
METHOD_LOOKUP_DECLARATION(object, OBJECT_METHODS)

#define CONTENTRESOLVER_METHODS(X)                                  \
  X(OpenAssetFileDescriptor, "openAssetFileDescriptor",             \
    "(Landroid/net/Uri;Ljava/lang/String;)"                         \
    "Landroid/content/res/AssetFileDescriptor;"),                   \
  X(Query, "query",                                                 \
    "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;"       \
    "[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;")
METHOD_LOOKUP_DECLARATION(content_resolver, CONTENTRESOLVER_METHODS)

#define ASSETFILEDESCRIPTOR_METHODS(X)                    \
  X(GetParcelFileDescriptor, "getParcelFileDescriptor",   \
    "()Landroid/os/ParcelFileDescriptor;")
METHOD_LOOKUP_DECLARATION(asset_file_descriptor, ASSETFILEDESCRIPTOR_METHODS)

#define PARCELFILEDESCRIPTOR_METHODS(X) X(DetachFd, "detachFd", "()I")
METHOD_LOOKUP_DECLARATION(parcel_file_descriptor, PARCELFILEDESCRIPTOR_METHODS)

#define CURSOR_METHODS(X)                                       \
  X(GetColumnIndex, "getColumnIndex", "(Ljava/lang/String;)I"), \
  X(GetInt, "getInt", "(I)I"),                                  \
  X(GetString, "getString", "(I)Ljava/lang/String;"),           \
  X(MoveToFirst, "moveToFirst", "()Z")
METHOD_LOOKUP_DECLARATION(cursor, CURSOR_METHODS)

class JavaThreadContext {
 public:
  typedef void (*Callback)(void* function_data);

  explicit JavaThreadContext(JNIEnv* env);
  ~JavaThreadContext();

  void Cancel();
  void ReleaseExecuteCancelLock();
  bool AcquireExecuteCancelLock();

  static bool Initialize(
      JNIEnv* env, jobject activity_object,
      const std::vector<internal::EmbeddedFile>& embedded_files);
  static void Terminate(JNIEnv* env);

  static void RunOnMainThread(JNIEnv* env, jobject activity_object,
                              Callback function_ptr, void* function_data,
                              Callback cancel_function_ptr,
                              JavaThreadContext* context);
  static void RunOnBackgroundThread(JNIEnv* env, Callback function_ptr,
                                    void* function_data,
                                    Callback cancel_function_ptr,
                                    JavaThreadContext* context);

 private:
  static jobject SetupInstance(JNIEnv* env, Callback function_ptr,
                               void* function_data,
                               Callback cancel_function_ptr,
                               JavaThreadContext* context);

  jni::Global<jobject> object_;
};

using jni::ExceptionClearGuard;
using jni::ExceptionPolicy;
using jni::GlobalRef;
using jni::JniCallbackScope;
using jni::JniEnv;
using jni::ScopedLocalRef;
using jni::ScopedThreadAttachment;

bool IsJArray(JNIEnv* env, jobject obj);
std::string JObjectClassName(JNIEnv* env, jobject obj);

jobject StdVectorToJavaList(JNIEnv* env,
                            const std::vector<std::string>& string_vector);
jobject StdUnorderedSetToJavaList(
    JNIEnv* env, const std::unordered_set<std::string>& string_set);
void StdMapToJavaMap(JNIEnv* env, jobject* to,
                     const std::map<const char*, const char*>& string_map);
void StdMapToJavaMap(JNIEnv* env, jobject* to,
                     const std::map<std::string, std::string>& from);
void JavaMapToStdMap(JNIEnv* env, std::map<std::string, std::string>* to,
                     jobject from);
void JavaMapToVariantMap(JNIEnv* env, std::map<Variant, Variant>* to,
                         jobject from);
void JavaSetToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                              jobject from);
void JavaListToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                               jobject from);
void JavaObjectListToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                                     jobject from);

std::string JniStringToString(JNIEnv* env, jobject string_object);
std::string JniObjectToString(JNIEnv* env, jobject obj);
std::string JStringToString(JNIEnv* env, jobject string_object);
std::string JniUriToString(JNIEnv* env, jobject uri);
std::string JniUriGetPath(JNIEnv* env, jobject uri);

bool JBooleanToBool(JNIEnv* env, jobject obj);
int8_t JByteToInt8(JNIEnv* env, jobject obj);
uint8_t JByteToUInt8(JNIEnv* env, jobject obj);
uint16_t JCharToUInt16(JNIEnv* env, jobject obj);
char JCharToChar(JNIEnv* env, jobject obj);
int16_t JShortToInt16(JNIEnv* env, jobject obj);
int JIntToInt(JNIEnv* env, jobject obj);
int JIntegerToInt(JNIEnv* env, jobject obj);
int64_t JLongToInt64(JNIEnv* env, jobject obj);
float JFloatToFloat(JNIEnv* env, jobject obj);
double JDoubleToDouble(JNIEnv* env, jobject obj);

jobject VariantToJavaObject(JNIEnv* env, const Variant& variant);
Variant JavaObjectToVariant(JNIEnv* env, jobject object);
Variant JBooleanArrayToVariant(JNIEnv* env, jbooleanArray array);
Variant JByteArrayToVariant(JNIEnv* env, jbyteArray array);
Variant JCharArrayToVariant(JNIEnv* env, jcharArray array);
Variant JShortArrayToVariant(JNIEnv* env, jshortArray array);
Variant JIntArrayToVariant(JNIEnv* env, jintArray array);
Variant JLongArrayToVariant(JNIEnv* env, jlongArray array);
Variant JFloatArrayToVariant(JNIEnv* env, jfloatArray array);
Variant JDoubleArrayToVariant(JNIEnv* env, jdoubleArray array);
Variant JObjectArrayToVariant(JNIEnv* env, jobjectArray array);

jobject CharsToJniUri(JNIEnv* env, const char* uri);
jobject ParseUriString(JNIEnv* env, const char* uri_string);
jobject CharsToURL(JNIEnv* env, const char* url_string);
std::vector<unsigned char> JniByteArrayToVector(JNIEnv* env, jobject array);
jbyteArray ByteBufferToJavaByteArray(JNIEnv* env, const uint8_t* data,
                                     size_t size);
jobject LocalToGlobalReference(JNIEnv* env, jobject local_reference);
jobject ContinueBuilder(JNIEnv* env, jobject old_builder, jobject new_builder);

enum FutureResult {
  kFutureResultSuccess,
  kFutureResultFailure,
  kFutureResultCancelled,
};

typedef void TaskCallbackFn(JNIEnv* env, jobject result,
                            FutureResult result_code,
                            const char* status_message, void* callback_data);

void RegisterCallbackOnTask(JNIEnv* env, jobject task,
                            TaskCallbackFn callback_fn, void* callback_data,
                            const char* api_identifier);
void CancelCallbacks(JNIEnv* env, const char* api_identifier);

jclass FindClassGlobal(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>* embedded_files,
    const char* class_name, ClassRequirement optional);
jclass FindClass(JNIEnv* env, const char* class_name);
inline jclass FindClass(JNIEnv* env, jobject activity_object,
                        const char* class_name) {
  (void)activity_object;
  return FindClass(env, class_name);
}

const std::vector<internal::EmbeddedFile>& CacheEmbeddedFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files);
jclass FindClassInFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files,
    const char* class_name);
int GetResourceIdFromActivity(JNIEnv* env, jobject activity_object,
                              const char* resource_name,
                              ResourceType resource_type);
std::string GetResourceStringFromActivity(JNIEnv* env, jobject activity_object,
                                          int resource_id);
std::string GetPackageName(JNIEnv* env, jobject activity_object);

void RunOnMainThread(JNIEnv* env, jobject activity_object,
                     JavaThreadContext::Callback function_ptr,
                     void* function_data,
                     JavaThreadContext::Callback cancel_function_ptr = nullptr,
                     JavaThreadContext* context = nullptr);
void RunOnBackgroundThread(
    JNIEnv* env, JavaThreadContext::Callback function_ptr, void* function_data,
    JavaThreadContext::Callback cancel_function_ptr = nullptr,
    JavaThreadContext* context = nullptr);

bool CheckAndClearJniExceptions(JNIEnv* env);
std::string GetAndClearExceptionMessage(JNIEnv* env);
std::string GetMessageFromException(JNIEnv* env, jobject exception);
bool LogException(JNIEnv* env, LogLevel log_level = kLogLevelError,
                  const char* log_fmt = nullptr, ...);
JNIEnv* GetThreadsafeJNIEnv(JavaVM* java_vm);
jint AttachCurrentThread(JavaVM* java_vm, JNIEnv** env);
JNIEnv* GetJNIEnvFromApp();

}  // namespace util
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_UTIL_ANDROID_H_
