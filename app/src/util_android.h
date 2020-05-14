/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_UTIL_ANDROID_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_UTIL_ANDROID_H_

#include <jni.h>
#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/jobject_reference.h"
#include "app/src/log.h"

// To ensure that Proguard doesn't strip the classes you're using, place this
// string directly before the JNI class string in your METHOD_LOOKUP_DEFINITION
// macro invocation.
#define PROGUARD_KEEP_CLASS "%PG%"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
// Workaround the inconsistent definition of JNINativeMethod w.r.t. const char*.
// Android JNI use const char* while Java JNI use char*. The latter has issue
// when casting implicitly from string literals in C++11.
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
typedef struct {
  const char* name;
  const char* signature;
  void* fnPtr;  // NOLINT
} JNINativeMethod;
// Converts FIREBASE_NAMESPACE::JNINativeMethod array to ::JNINativeMethod
// array.
// Caller ownes the result and must call CleanUpConvertedJNINativeMethod() to
// release the allocation.
inline const ::JNINativeMethod* ConvertJNINativeMethod(
    const JNINativeMethod* source, size_t number_of_native_methods) {
  ::JNINativeMethod* target = new ::JNINativeMethod[number_of_native_methods];
  ::JNINativeMethod* result = target;
  for (int i = 0; i < number_of_native_methods; ++i) {
    target->name = const_cast<char*>(source->name);
    target->signature = const_cast<char*>(source->signature);
    target->fnPtr = source->fnPtr;
    ++source;
    ++target;
  }
  return result;
}
inline void CleanUpConvertedJNINativeMethod(const ::JNINativeMethod* target) {
  // It is not const; we created it in ConvertJNINativeMethod().
  delete[] const_cast<::JNINativeMethod*>(target);
}
#else   // defined(FIREBASE_ANDROID_FOR_DESKTOP)
typedef ::JNINativeMethod JNINativeMethod;
// Do nothing. Caller does not actually own the result but can call the no-op
// CleanUpConvertedJNINativeMethod() to match the other branch.
inline const ::JNINativeMethod* ConvertJNINativeMethod(
    const JNINativeMethod* source, size_t number_of_native_methods) {
  // We avoid copying and use the source directly.
  return source;
}
inline void CleanUpConvertedJNINativeMethod(const ::JNINativeMethod* target) {
  // Dothing, as caller does not own it.
  return;
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

namespace util {

// Type of a Java method.
enum MethodType {
  kMethodTypeInstance,  // Called on an instance of a class.
  kMethodTypeStatic,    // Static method of a class.
};

// Whether a Java method (or field) is required or optional.
enum MethodRequirement {
  kMethodRequired,  // The method is required, an assert will occur if you try
                    // to look it up and it doesn't exist.
  kMethodOptional,  // The method is optional, no error will occur if you try
                    // to look it up and it doesn't exist.
};

// Name and signature of a class method.
struct MethodNameSignature {
  const char* name;       // Name of the method e.g java/lang/Object
  const char* signature;  // JNI signature of the method e.g "()I"
  const MethodType type;
  const MethodRequirement optional;  // Whether lookup of this method should
                                     // fail silently.
};

// Whether this is instance or static field in a class.
enum FieldType {
  kFieldTypeInstance,  // Field on an instance of a class.
  kFieldTypeStatic,    // Static field of a class.
};

// Name and type of a class field.
struct FieldDescriptor {
  const char* name;       // Name of the field.
  const char* signature;  // JNI string that describes the type.
  const FieldType type;
  const MethodRequirement optional;  // Whether lookup of this field should fail
                                     // silently.
};

// Whether a Java class is required or optional.
enum ClassRequirement {
  kClassRequired,  // The class is required, an error will be logged if you
                   // try to look it up and it doesn't exist.
  kClassOptional,  // The class is optional, no error be logged if you try
                   // to look it up and it doesn't exist.
};

// Maps ResourceType enumeration values to strings.
// clang-format off
#define RESOURCE_TYPES(X) \
  X(String, "string"), \
  X(Id, "id")
// clang-format on
#define RESOURCE_TYPE_ENUM(type_identifier, type_name) \
  kResourceType##type_identifier
#define RESOURCE_TYPE_STRING(type_identifier, type_name) type_name

// Android Resource types.
// See http://developer.android.com/guide/topics/resources/
// NOTE: This is not an exhaustive set of all resource types.
enum ResourceType { RESOURCE_TYPES(RESOURCE_TYPE_ENUM) };

// Initialize the utilities library. It is safe to call this multiple times
// though each call should be paired with a Terminate() as the initialization
// is reference counted.
bool Initialize(JNIEnv* env, jobject activity_object);

// This causes only a partial initialization of this module. It can be used
// instead of "Initialize" and will support only very basic use of "FindClass",
// and none of the other API methods in this module. This is highly specialized
// to provide only the Activity Class Loader. It's intended to be used when
// only the Activity Class Loader is needed, and in that case can initialize
// much more quickly as the full initialization requires reading other classes
// from the disk and compiling them.
// Calls to InitializeActivityClasses should be paired with a corresponding call
// to TerminateActivityClasses.
bool InitializeActivityClasses(JNIEnv* env, jobject activity_object);

// Terminate the utilities library.  Releases all global references to
// classes.
void Terminate(JNIEnv* env);

// Terminate the activity class loader that was setup with
// InitializeActivityClasses.
void TerminateActivityClasses(JNIEnv* env);

// Lookup method IDs specified by the method_name_signatures array and store
// in method_ids.  Used by METHOD_LOOKUP_DECLARATION.
bool LookupMethodIds(JNIEnv* env, jclass clazz,
                     const MethodNameSignature* method_name_signatures,
                     size_t number_of_method_name_signatures,
                     jmethodID* method_ids, const char* class_name);

// Lookup field IDs specified by the field_descriptors array and store
// in field_ids.  Used by FIELD_LOOKUP_DECLARATION.
bool LookupFieldIds(JNIEnv* env, jclass clazz,
                    const FieldDescriptor* field_descriptors,
                    size_t number_of_field_descriptors, jfieldID* field_ids,
                    const char* class_name);

// Used to make METHOD_ID and METHOD_NAME_SIGNATURE macros variadic.
#define METHOD_NAME_INFO_EXPANDER(arg0, arg1, arg2, arg3, arg4, arg5, \
                                  function, ...)                      \
  function

// Used to populate an array of MethodNameSignature.
#define METHOD_NAME_SIGNATURE_5(id, name, signature, method_type, optional) \
  { name, signature, method_type, optional }
#define METHOD_NAME_SIGNATURE_4(id, name, signature, method_type) \
  METHOD_NAME_SIGNATURE_5(                                        \
      id, name, signature, method_type,                           \
      ::FIREBASE_NAMESPACE::util::MethodRequirement::kMethodRequired)
#define METHOD_NAME_SIGNATURE_3(id, name, signature) \
  METHOD_NAME_SIGNATURE_4(id, name, signature,       \
                          ::FIREBASE_NAMESPACE::util::kMethodTypeInstance)
#define METHOD_NAME_SIGNATURE(...)                                        \
  METHOD_NAME_INFO_EXPANDER(, ##__VA_ARGS__,                              \
                            METHOD_NAME_SIGNATURE_5(__VA_ARGS__),         \
                            METHOD_NAME_SIGNATURE_4(__VA_ARGS__),         \
                            METHOD_NAME_SIGNATURE_3(__VA_ARGS__),         \
                            numargs2(__VA_ARGS__), numargs1(__VA_ARGS__), \
                            /* Generate nothing with no args. */)

// Used to populate an enum of method identifiers.
#define METHOD_ID_3(id, name, signature) k##id
#define METHOD_ID_4(id, name, signature, method_type) k##id
#define METHOD_ID_5(id, name, signature, method_type, optional) k##id
#define METHOD_ID(...)                                                        \
  METHOD_NAME_INFO_EXPANDER(                                                  \
      , ##__VA_ARGS__, METHOD_ID_5(__VA_ARGS__), METHOD_ID_4(__VA_ARGS__),    \
      METHOD_ID_3(__VA_ARGS__), numargs2(__VA_ARGS__), numargs1(__VA_ARGS__), \
      /* Generate nothing with no args. */)

// Used to populate FieldDescriptor
#define FIELD_DESCRIPTOR(...) METHOD_NAME_SIGNATURE(__VA_ARGS__)
// Used to populate an enum of field identifiers.
#define FIELD_ID(...) METHOD_ID(__VA_ARGS__)

// Used with METHOD_LOOKUP_DECLARATION to generate no method lookups.
#define METHOD_LOOKUP_NONE(X)                                  \
  X(InvalidMethod, nullptr, nullptr,                           \
    ::FIREBASE_NAMESPACE::util::MethodType::kMethodTypeStatic, \
    ::FIREBASE_NAMESPACE::util::MethodRequirement::kMethodOptional)

// Used with METHOD_LOOKUP_DECLARATION to generate no field lookups.
#define FIELD_LOOKUP_NONE(X)                                 \
  X(InvalidField, nullptr, nullptr,                          \
    ::FIREBASE_NAMESPACE::util::FieldType::kFieldTypeStatic, \
    ::FIREBASE_NAMESPACE::util::MethodRequirement::kMethodOptional)

// Declares a namespace which caches class method IDs.
// To make cached method IDs available in to other files or projects, this macro
// must be defined in a header file. If the method IDs are local to a specific
// file however, this can be placed in the source file. Regardless of whether
// this is placed in the header or not, you must also add
// METHOD_LOOKUP_DEFINITION to the source file.
// clang-format off
#define METHOD_LOOKUP_DECLARATION_3(namespace_identifier,                     \
                                    method_descriptor_macro,                  \
                                    field_descriptor_macro)                   \
  namespace namespace_identifier {                                            \
                                                                              \
  enum Method {                                                               \
    method_descriptor_macro(METHOD_ID),                                       \
    kMethodCount                                                              \
  };                                                                          \
                                                                              \
  enum Field {                                                                \
    field_descriptor_macro(FIELD_ID),                                         \
    kFieldCount                                                               \
  };                                                                          \
                                                                              \
  /* Find and hold a reference to this namespace's class, optionally */       \
  /* searching a set of files for the class. */                               \
  /* This specified file list must have previously been cached using */       \
  /* CacheEmbeddedFiles(). */                                                 \
  jclass CacheClassFromFiles(                                                 \
      JNIEnv *env, jobject activity_object,                                   \
      const std::vector<::FIREBASE_NAMESPACE::internal::EmbeddedFile>*        \
          embedded_files);                                                    \
                                                                              \
  /* Find and hold a reference to this namespace's class. */                  \
  jclass CacheClass(JNIEnv *env);                                             \
                                                                              \
  /* Get the cached class associated with this namespace. */                  \
  jclass GetClass();                                                          \
                                                                              \
  /* Register native methods on the class associated with this namespace. */  \
  bool RegisterNatives(JNIEnv* env, const JNINativeMethod* native_methods,    \
                       size_t number_of_native_methods);                      \
                                                                              \
  /* Release the cached class reference. */                                   \
  void ReleaseClass(JNIEnv *env);                                             \
                                                                              \
  /* See LookupMethodIds() */                                                 \
  bool CacheMethodIds(JNIEnv *env, jobject activity_object);                  \
                                                                              \
  /* Lookup a method ID using a Method enum value. */                         \
  jmethodID GetMethodId(Method method);                                       \
                                                                              \
  /* See LookupFieldIds() */                                                  \
  bool CacheFieldIds(JNIEnv *env, jobject activity_object);                   \
                                                                              \
  /* Lookup a field ID using a Field enum value. */                           \
  jfieldID GetFieldId(Field field);                                           \
                                                                              \
  }  // namespace namespace_identifier

#define METHOD_LOOKUP_DECLARATION_2(namespace_identifier,                     \
                                    method_descriptor_macro)                  \
  METHOD_LOOKUP_DECLARATION_3(namespace_identifier, method_descriptor_macro,  \
                              FIELD_LOOKUP_NONE)

// Used to make METHOD_LOOKUP_DECLARATION variadic.
#define METHOD_LOOKUP_DECLARATION_EXPANDER(arg0, arg1, arg2, arg3,            \
                                           function, ...)                     \
  function

#define METHOD_LOOKUP_DECLARATION(...)                                        \
  METHOD_LOOKUP_DECLARATION_EXPANDER(                                         \
      , ##__VA_ARGS__, METHOD_LOOKUP_DECLARATION_3(__VA_ARGS__),              \
      METHOD_LOOKUP_DECLARATION_2(__VA_ARGS__), numargs1(__VA_ARGS__),        \
      numargs0(__VA_ARGS__))

// Defines a namespace which caches class method IDs.
// To cache class method IDs, you must first declare them in either the header
// or source file with METHOD_LOOKUP_DECLARATION. Regardless of whether the
// declaration is in a header or source file, this macro must be called from a
// source file.
// clang-format off
#define METHOD_LOOKUP_DEFINITION_4(namespace_identifier,                      \
                                   class_name, method_descriptor_macro,       \
                                   field_descriptor_macro)                    \
  namespace namespace_identifier {                                            \
                                                                              \
  /* Skip optional "%PG%" at the beginning of the string, if present. */      \
  static const char* kClassName =                                             \
      class_name[0] == '%' ? &class_name[4] : class_name;                     \
                                                                              \
  static const ::FIREBASE_NAMESPACE::util::MethodNameSignature                \
      kMethodSignatures[] = {                                                 \
    method_descriptor_macro(METHOD_NAME_SIGNATURE),                           \
  };                                                                          \
                                                                              \
  static const ::FIREBASE_NAMESPACE::util::FieldDescriptor                    \
      kFieldDescriptors[] = {                                                 \
    field_descriptor_macro(FIELD_DESCRIPTOR),                                 \
  };                                                                          \
                                                                              \
  static jmethodID g_method_ids[kMethodCount];                                \
  static jfieldID g_field_ids[kFieldCount];                                   \
                                                                              \
  static jclass g_class = nullptr;                                            \
  static bool g_registered_natives = false;                                   \
                                                                              \
  /* Find and hold a reference to this namespace's class, optionally */       \
  /* searching a set of files for the class. */                               \
  /* This specified file list must have previously been cached using */       \
  /* CacheEmbeddedFiles(). */                                                 \
  /* If optional == kClassOptional, no errors will be emitted if the class */ \
  /* does not exist. */                                                       \
  jclass CacheClassFromFiles(                                                 \
      JNIEnv *env, jobject activity_object,                                   \
      const std::vector<::FIREBASE_NAMESPACE::internal::EmbeddedFile>*        \
          embedded_files,                                                     \
      ::FIREBASE_NAMESPACE::util::ClassRequirement optional) {                \
    if (!g_class) {                                                           \
      g_class = ::FIREBASE_NAMESPACE::util::FindClassGlobal(                  \
          env, activity_object, embedded_files, kClassName, optional);        \
    }                                                                         \
    return g_class;                                                           \
  }                                                                           \
                                                                              \
  jclass CacheClassFromFiles(                                                 \
      JNIEnv *env, jobject activity_object,                                   \
      const std::vector<::FIREBASE_NAMESPACE::internal::EmbeddedFile>*        \
          embedded_files) {                                                   \
    return CacheClassFromFiles(                                               \
        env, activity_object, embedded_files,                                 \
        ::FIREBASE_NAMESPACE::util::ClassRequirement::kClassRequired);        \
  }                                                                           \
                                                                              \
  /* Find and hold a reference to this namespace's class. */                  \
  jclass CacheClass(JNIEnv* env, jobject activity_object,                     \
                    ::FIREBASE_NAMESPACE::util::ClassRequirement optional) {  \
    return CacheClassFromFiles(env, activity_object, nullptr, optional);      \
  }                                                                           \
                                                                              \
  /* Find and hold a reference to this namespace's class. */                  \
  jclass CacheClass(JNIEnv* env, jobject activity_object) {                   \
    return CacheClassFromFiles(                                               \
        env, activity_object, nullptr,                                        \
        ::FIREBASE_NAMESPACE::util::ClassRequirement::kClassRequired);        \
  }                                                                           \
                                                                              \
  /* Get the cached class associated with this namespace. */                  \
  jclass GetClass() { return g_class; }                                       \
                                                                              \
  bool RegisterNatives(JNIEnv* env, const JNINativeMethod* native_methods,    \
                       size_t number_of_native_methods) {                     \
    if (g_registered_natives) return false;                                   \
    const ::JNINativeMethod* true_native_methods =                            \
        FIREBASE_NAMESPACE::ConvertJNINativeMethod(native_methods,            \
                                         number_of_native_methods);           \
    const jint register_status = env->RegisterNatives(                        \
      GetClass(), true_native_methods, number_of_native_methods);             \
    FIREBASE_NAMESPACE::CleanUpConvertedJNINativeMethod(true_native_methods); \
    FIREBASE_NAMESPACE::util::CheckAndClearJniExceptions(env);                \
    g_registered_natives = register_status == JNI_OK;                         \
    return g_registered_natives;                                              \
  }                                                                           \
                                                                              \
  /* Release the cached class reference. */                                   \
  void ReleaseClass(JNIEnv* env) {                                            \
    if (g_class) {                                                            \
      if (g_registered_natives) {                                             \
        env->UnregisterNatives(g_class);                                      \
        g_registered_natives = false;                                         \
      }                                                                       \
      FIREBASE_NAMESPACE::util::CheckAndClearJniExceptions(env);              \
      env->DeleteGlobalRef(g_class);                                          \
      g_class = nullptr;                                                      \
    }                                                                         \
  }                                                                           \
                                                                              \
  /* See LookupMethodIds() */                                                 \
  /* If the class is being loaded from an embedded file use */                \
  /* CacheClassFromFiles() before calling this function to cache method */    \
  /* IDs. */                                                                  \
  bool CacheMethodIds(JNIEnv* env, jobject activity_object) {                 \
    return ::FIREBASE_NAMESPACE::util::LookupMethodIds(                       \
        env, CacheClass(env, activity_object), kMethodSignatures,             \
        kMethodCount, g_method_ids, kClassName);                              \
  }                                                                           \
                                                                              \
  /* Lookup a method ID using a Method enum value. */                         \
  jmethodID GetMethodId(Method method) {                                      \
    FIREBASE_ASSERT(method < kMethodCount);                                   \
    jmethodID method_id = g_method_ids[method];                               \
    return method_id;                                                         \
  }                                                                           \
                                                                              \
  /* See LookupFieldIds() */                                                  \
  /* If the class is being loaded from an embedded file use */                \
  /* CacheClassFromFiles() before calling this function to cache field */     \
  /* IDs. */                                                                  \
  bool CacheFieldIds(JNIEnv* env, jobject activity_object) {                  \
    return ::FIREBASE_NAMESPACE::util::LookupFieldIds(                        \
        env, CacheClass(env, activity_object),                                \
        kFieldDescriptors, kFieldCount,                                       \
        g_field_ids, kClassName);                                             \
  }                                                                           \
                                                                              \
  /* Lookup a field ID using a Field enum value. */                           \
  jfieldID GetFieldId(Field field) {                                          \
    FIREBASE_ASSERT(field < kFieldCount);                                     \
    jfieldID field_id = g_field_ids[field];                                   \
    return field_id;                                                          \
  }                                                                           \
                                                                              \
  }  // namespace namespace_identifier
// clang-format on

#define METHOD_LOOKUP_DEFINITION_3(namespace_identifier, class_name, \
                                   method_descriptor_macro)          \
  METHOD_LOOKUP_DEFINITION_4(namespace_identifier, class_name,       \
                             method_descriptor_macro, FIELD_LOOKUP_NONE)

// Used to make METHOD_LOOKUP_DEFINITION variadic.
#define METHOD_LOOKUP_DEFINITION_EXPANDER(arg0, arg1, arg2, arg3, arg4, \
                                          function, ...)                \
  function

#define METHOD_LOOKUP_DEFINITION(...)                                 \
  METHOD_LOOKUP_DEFINITION_EXPANDER(                                  \
      , ##__VA_ARGS__, METHOD_LOOKUP_DEFINITION_4(__VA_ARGS__),       \
      METHOD_LOOKUP_DEFINITION_3(__VA_ARGS__), numargs2(__VA_ARGS__), \
      numargs1(__VA_ARGS__), numargs0(__VA_ARGS))

// Used to call android.app.Activity methods.
// clang-format off
#define ACTIVITY_BASE_METHODS(X)                                        \
  X(GetApplicationContext, "getApplicationContext",                     \
  "()Landroid/content/Context;"),                                       \
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
#define ACTIVITY_METHODS(X)                                             \
  ACTIVITY_BASE_METHODS(X),                                             \
  X(GetCodeCacheDir, "getCodeCacheDir", "()Ljava/io/File;",             \
    util::kMethodTypeInstance, util::kMethodOptional)
#else
#define ACTIVITY_METHODS(X) ACTIVITY_BASE_METHODS(X)
#endif  // !defined(FIREBASE_ANDROID_FOR_DESKTOP)
// clang-format on
METHOD_LOOKUP_DECLARATION(activity, ACTIVITY_METHODS)

// Used to setup the cache of Bundle class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define BUNDLE_METHODS(X)                                                \
    X(Constructor, "<init>", "()V"),                                     \
    X(GetString, "getString", "(Ljava/lang/String;)Ljava/lang/String;"), \
    X(KeySet, "keySet", "()Ljava/util/Set;"),                            \
    X(PutFloat, "putFloat", "(Ljava/lang/String;F)V"),                   \
    X(PutLong, "putLong", "(Ljava/lang/String;J)V"),                     \
    X(PutString, "putString", "(Ljava/lang/String;Ljava/lang/String;)V")
// clang-format on
METHOD_LOOKUP_DECLARATION(bundle, BUNDLE_METHODS)

// Used for java.lang.Class to check if a Java Object is an array.
// clang-format off
#define CLASS_METHODS(X)                                                 \
    X(IsArray, "isArray", "()Z"),                                        \
    X(GetName, "getName", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(class_class, CLASS_METHODS)

// Used to setup the cache of Iterator class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define ITERATOR_METHODS(X)    \
    X(HasNext, "hasNext", "()Z"), \
    X(Next, "next", "()Ljava/lang/Object;")
// clang-format on
METHOD_LOOKUP_DECLARATION(iterator, ITERATOR_METHODS)

// clang-format off
#define ITERABLE_METHODS(X)    \
    X(Iterator, "iterator", "()Ljava/util/Iterator;")
// clang-format on
METHOD_LOOKUP_DECLARATION(iterable, ITERABLE_METHODS)

// Used to setup the cache of Set class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define SET_METHODS(X) \
    X(Iterator, "iterator", "()Ljava/util/Iterator;")
// clang-format on
METHOD_LOOKUP_DECLARATION(set, SET_METHODS)

// Used to read from java.util.List containers.
// clang-format off
#define LIST_METHODS(X)                                         \
    X(Get, "get", "(I)Ljava/lang/Object;"),                     \
    X(Set, "set", "(ILjava/lang/Object;)Ljava/lang/Object;"),   \
    X(Size, "size", "()I")
// clang-format on
METHOD_LOOKUP_DECLARATION(list, LIST_METHODS)

// Used to create ArrayList instances.
// clang-format off
#define ARRAY_LIST_METHODS(X)                       \
  X(Constructor, "<init>", "()V"),                  \
  X(ConstructorWithSize, "<init>", "(I)V"),         \
  X(Add, "add", "(Ljava/lang/Object;)Z")
// clang-format on
METHOD_LOOKUP_DECLARATION(array_list, ARRAY_LIST_METHODS)

// clang-format off
#define INTENT_METHODS(X)                                                     \
    X(Constructor, "<init>",                                                  \
      "(Landroid/content/Context;Ljava/lang/Class;)V"),                       \
    X(PutExtraString, "putExtra",                                             \
      "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;"),      \
    X(GetExtras, "getExtras", "()Landroid/os/Bundle;"),                       \
    X(GetIntExtra, "getIntExtra", "(Ljava/lang/String;I)I"),                  \
    X(GetData, "getData", "()Landroid/net/Uri;")
// clang-format on
METHOD_LOOKUP_DECLARATION(intent, INTENT_METHODS);

// clang-format off
#define FILE_METHODS(X)                                                    \
  X(ConstructorFilePath, "<init>", "(Ljava/io/File;Ljava/lang/String;)V"), \
  X(GetAbsolutePath, "getAbsolutePath", "()Ljava/lang/String;"),           \
  X(GetPath, "getPath", "()Ljava/lang/String;"),                           \
  X(ToUri, "toURI", "()Ljava/net/URI;")
// clang-format on
METHOD_LOOKUP_DECLARATION(file, FILE_METHODS)

// clang-format off
#define CONTEXT_METHODS(X)                             \
    X(GetFilesDir, "getFilesDir", "()Ljava/io/File;"), \
    X(StartService, "startService",                    \
      "(Landroid/content/Intent;)Landroid/content/ComponentName;"), \
    X(GetPackageName, "getPackageName", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(context, CONTEXT_METHODS);

// clang-format off
#define DATE_METHODS(X)                       \
    X(Constructor, "<init>", "()V"),          \
    X(ConstructorWithTime, "<init>", "(J)V"), \
    X(GetTime, "getTime", "()J")
// clang-format on
METHOD_LOOKUP_DECLARATION(date, DATE_METHODS);

// clang-format off
#define MAP_METHODS(X)                                                         \
    X(Put, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"), \
    X(Get, "get", "(Ljava/lang/Object;)Ljava/lang/Object;"),                   \
    X(KeySet, "keySet", "()Ljava/util/Set;")
// clang-format on
METHOD_LOOKUP_DECLARATION(map, MAP_METHODS);

// clang-format off
#define HASHMAP_METHODS(X) \
    X(Constructor, "<init>", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(hash_map, HASHMAP_METHODS);

// clang-format off
#define DOUBLE_METHODS(X)             \
    X(Constructor, "<init>", "(D)V"), \
    X(Value, "doubleValue", "()D")
// clang-format on
METHOD_LOOKUP_DECLARATION(double_class, DOUBLE_METHODS);

// clang-format off
#define BOOLEAN_METHODS(X)            \
    X(Constructor, "<init>", "(Z)V"), \
    X(Value, "booleanValue", "()Z")
// clang-format on
METHOD_LOOKUP_DECLARATION(boolean_class, BOOLEAN_METHODS);

// clang-format off
#define LONG_METHODS(X)               \
    X(Constructor, "<init>", "(J)V"), \
    X(Value, "longValue", "()J")
// clang-format on
METHOD_LOOKUP_DECLARATION(long_class, LONG_METHODS);

// clang-format off
#define BYTE_METHODS(X)               \
    X(Constructor, "<init>", "(B)V"), \
    X(Value, "byteValue", "()B")
// clang-format on
METHOD_LOOKUP_DECLARATION(byte_class, BYTE_METHODS);

// clang-format off
#define CHARACTER_METHODS(X)          \
    X(Constructor, "<init>", "(C)V"), \
    X(Value, "charValue", "()C")
// clang-format on
METHOD_LOOKUP_DECLARATION(character_class, CHARACTER_METHODS);

// clang-format off
#define FLOAT_METHODS(X)              \
    X(Constructor, "<init>", "(F)V"), \
    X(Value, "floatValue", "()F")
// clang-format on
METHOD_LOOKUP_DECLARATION(float_class, FLOAT_METHODS);

// clang-format off
#define INTEGER_METHODS(X)             \
    X(Constructor, "<init>", "(I)V"),  \
    X(Value, "intValue", "()I")
// clang-format on
METHOD_LOOKUP_DECLARATION(integer_class, INTEGER_METHODS);

// clang-format off
#define SHORT_METHODS(X)               \
    X(Constructor, "<init>", "(S)V"),  \
    X(Value, "shortValue", "()S")
// clang-format on
METHOD_LOOKUP_DECLARATION(short_class, SHORT_METHODS);

// clang-format off
#define STRING_METHODS(X)               \
    X(Constructor, "<init>", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(string, STRING_METHODS);

// clang-format off
#define THROWABLE_METHODS(X)               \
    X(GetLocalizedMessage, "getLocalizedMessage", "()Ljava/lang/String;"), \
    X(GetMessage, "getMessage", "()Ljava/lang/String;"),                   \
    X(ToString, "toString", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(throwable, THROWABLE_METHODS);

// Methods of the android.net.Uri class,
// clang-format off
#define URI_METHODS(X)                                                         \
    X(ToString, "toString", "()Ljava/lang/String;"),                           \
    X(Parse, "parse", "(Ljava/lang/String;)Landroid/net/Uri;",                 \
      kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(uri, URI_METHODS)

// Methods of the java.lang.Object class,
// clang-format off
#define OBJECT_METHODS(X)                                                      \
    X(ToString, "toString", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(object, OBJECT_METHODS)

// clang-format off
#define CONTENTRESOLVER_METHODS(X)                                \
    X(OpenAssetFileDescriptor, "openAssetFileDescriptor",         \
      "(Landroid/net/Uri;Ljava/lang/String;)"                     \
      "Landroid/content/res/AssetFileDescriptor;"),               \
    X(Query, "query",                                             \
      "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;"   \
      "[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;")
// clang-format on
METHOD_LOOKUP_DECLARATION(content_resolver, CONTENTRESOLVER_METHODS)

// clang-format off
#define ASSETFILEDESCRIPTOR_METHODS(X)                     \
    X(GetParcelFileDescriptor, "getParcelFileDescriptor",  \
      "()Landroid/os/ParcelFileDescriptor;")
// clang-format on
METHOD_LOOKUP_DECLARATION(asset_file_descriptor, ASSETFILEDESCRIPTOR_METHODS)

// clang-format off
#define PARCELFILEDESCRIPTOR_METHODS(X)  \
    X(DetachFd, "detachFd", "()I")
// clang-format on
METHOD_LOOKUP_DECLARATION(parcel_file_descriptor, PARCELFILEDESCRIPTOR_METHODS)

// clang-format off
#define CURSOR_METHODS(X)  \
    X(GetColumnIndex, "getColumnIndex", "(Ljava/lang/String;)I"), \
    X(GetInt, "getInt", "(I)I"),                                  \
    X(GetString, "getString", "(I)Ljava/lang/String;"),           \
    X(MoveToFirst, "moveToFirst", "()Z")
// clang-format on
METHOD_LOOKUP_DECLARATION(cursor, CURSOR_METHODS)

// Holds a reference to a Java thread that will execute a C++ method.
//
// To support cancelation (i.e. using ReleaseExecuteCancelLock() and
// AcquireExecuteCancelLock()) requires this object is accessed as a shared
// resource that is not destroyed until either the execution or cancelation
// method are complete.
class JavaThreadContext {
 public:
  // Function executed on the Java thread.
  // function_data is a pointer supplied via the RunOnMainThread() or
  // RunOnBackgroundThread() methods.
  typedef void (*Callback)(void* function_data);

  explicit JavaThreadContext(JNIEnv* env);
  ~JavaThreadContext();

  // Cancel the C++ callback from the Java thread context.
  void Cancel();

  // Release the execution / cancelation lock.
  // This allows other threads to signal cancellation of the current thread's
  // operation.
  // NOTE: This can only be called from the thread being executed.
  // A typical usage pattern is:
  //
  // util::RunOnBackgroundThread(env, [](void* function_data) {
  //     SharedPtr<JavaThreadContext> context =
  //       *(static_cast<SharedPtr<JavaThreadContext>*>(function_data));
  //     context->ReleaseExecuteCancelLock();
  //     // Perform a slow operation.
  //     if (context->AcquireExecuteCancelLock()) {
  //       // Return results.
  //     }
  //   });
  void ReleaseExecuteCancelLock();

  // Acquire the execution / cancelation lock.
  // This must be called after ReleaseExecuteCancelLock() for the current thread
  // to access this object's state.
  // This returns true if the lock was acquired, false if the thread was
  // canceled.
  bool AcquireExecuteCancelLock();

  // Cache classes for this module.
  static bool Initialize(
      JNIEnv* env, jobject activity_object,
      const std::vector<internal::EmbeddedFile>& embedded_files);
  // Discard classes for this module.
  static void Terminate(JNIEnv* env);

  // Run a C++ function on the main/UI thread.
  static void RunOnMainThread(JNIEnv* env, jobject activity_object,
                              Callback function_ptr, void* function_data,
                              Callback cancel_function_ptr,
                              JavaThreadContext* context);

  // Run a C++ function on a background thread.
  static void RunOnBackgroundThread(JNIEnv* env, Callback function_ptr,
                                    void* function_data,
                                    Callback cancel_function_ptr,
                                    JavaThreadContext* context);

 private:
  // Create a CppThreadDispatcher and optionally attach it to the
  // specified JavaContext object.  This method returns a local reference to the
  // CppThreadDispatcher which must be deleted after use.
  static jobject SetupInstance(JNIEnv* env, Callback function_ptr,
                               void* function_data,
                               Callback cancel_function_ptr,
                               JavaThreadContext* context);

 private:
  internal::JObjectReference object_;

  static int initialize_count_;
};

// Returns true of `obj` is a Java array.
bool IsJArray(JNIEnv* env, jobject obj);

// Return the name of the class of obj.
std::string JObjectClassName(JNIEnv* env, jobject obj);

// Converts a `std::vector<std::string>` to a `java.util.ArrayList<String>`
// Returns a local ref to a List.
jobject StdVectorToJavaList(JNIEnv* env,
                            const std::vector<std::string>& string_vector);

// Converts an `std::map<const char*, const char*>` to a
// `java.util.Map<String, String>`.
void StdMapToJavaMap(JNIEnv* env, jobject* to,
                     const std::map<const char*, const char*>& string_map);

// Converts an `std::map<std::string, std::string>` to a
// `java.util.Map<String, String>`.
void StdMapToJavaMap(JNIEnv* env, jobject* to,
                     const std::map<std::string, std::string>& from);

// Converts a `java.util.Map<String, String>` to an
// `std::map<std::string, std::string>`.
void JavaMapToStdMap(JNIEnv* env, std::map<std::string, std::string>* to,
                     jobject from);

// Converts a `java.util.Map<java.lang.Object, java.lang.Object>` to an
// `std::map<Variant, Variant>`.
void JavaMapToVariantMap(JNIEnv* env, std::map<Variant, Variant>* to,
                         jobject from);

// Converts a `java.util.Set<String>` to a `std::vector<std::string>`.
void JavaSetToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                              jobject from);

// Converts a `java.util.List<String>` to a `std::vector<std::string>`.
void JavaListToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                               jobject from);

// Converts a `java.util.List<Object>` to a `std::vector<std::string>`,
// calling toString() on each object first.
void JavaObjectListToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                                     jobject from);

// Convert a jstring to a std::string, releasing the reference to the jstring.
std::string JniStringToString(JNIEnv* env, jobject string_object);

// Convert a Java object of type java.lang.Object into an std::string by
// calling toString(), then release the object.
std::string JniObjectToString(JNIEnv* env, jobject obj);

// Convert a jstring (created by the JVM e.g passed into a native method) into
// a std::string.  Unlike JniStringToString() this does not release the
// reference to the string_object as the caller owns the object in a native
// method.
std::string JStringToString(JNIEnv* env, jobject string_object);

// Convert a jobject of type android.net.Uri into an std::string,
// and releases the reference to the jobject.
std::string JniUriToString(JNIEnv* env, jobject uri);

// Gets the path part from a jni object of an android.net.Uri as an std::string,
// and releases the reference to the jobject.
std::string JniUriGetPath(JNIEnv* env, jobject uri);

// Convert a Java Boolean object into a C++ bool.
// Note that a Java Boolean object is different from a jboolean primitive type.
bool JBooleanToBool(JNIEnv* env, jobject obj);

// Convert a Java Byte object into a C++ uint8_t.
// Note that a Java Byte object is different from a jbyte primitive type.
uint8_t JByteToUInt8(JNIEnv* env, jobject obj);

// Convert a Java Character object into a C++ char.
// Note that a Java Char object is different from a jchar primitive type.
char JCharToChar(JNIEnv* env, jobject obj);

// Convert a Java Short object into a C++ int16_t.
// Note that a Java Short object is different from a jshort primitive type.
int16_t JShortToInt16(JNIEnv* env, jobject obj);

// Convert a Java Integer object into a C++ int.
// Note that a Java Integer object is different from a jint primitive type.
int JIntegerToInt(JNIEnv* env, jobject obj);

// Convert a Java Long object into a C++ int64_t.
// Note that a Java Long object is different from a jlong primitive type.
int64_t JLongToInt64(JNIEnv* env, jobject obj);

// Convert a Java Float object into a C++ float.
// Note that a Java Float object is different from a jfloat primitive type.
float JFloatToFloat(JNIEnv* env, jobject obj);

// Convert a Java Double object into a C++ double.
// Note that a Java Double object is different from a jdouble primitive type.
double JDoubleToDouble(JNIEnv* env, jobject obj);

// Convert our Variant class into a generic Java object.
// Can be recursive. That is, Variant might be a map of Variants, which are
// vector of Variants, etc.
jobject VariantToJavaObject(JNIEnv* env, const Variant& variant);

// Convert a generic Java object into our Variant class.
// Can be recursive. That is, Variant might be a map of Variants, which are
// array of Variants, etc.
Variant JavaObjectToVariant(JNIEnv* env, jobject object);

// Convert a Java array of type jboolean into a Variant that holds a vector.
Variant JBooleanArrayToVariant(JNIEnv* env, jbooleanArray array);

// Convert a Java array of type jbyte into a Variant that holds a vector.
Variant JByteArrayToVariant(JNIEnv* env, jbyteArray array);

// Convert a Java array of type jchar into a Variant that holds a vector.
Variant JCharArrayToVariant(JNIEnv* env, jcharArray array);

// Convert a Java array of type jshort into a Variant that holds a vector.
Variant JShortArrayToVariant(JNIEnv* env, jshortArray array);

// Convert a Java array of type jint into a Variant that holds a vector.
Variant JIntArrayToVariant(JNIEnv* env, jintArray array);

// Convert a Java array of type jlong into a Variant that holds a vector.
Variant JLongArrayToVariant(JNIEnv* env, jlongArray array);

// Convert a Java array of type jfloat into a Variant that holds a vector.
Variant JFloatArrayToVariant(JNIEnv* env, jfloatArray array);

// Convert a Java array of type jdouble into a Variant that holds a vector.
Variant JDoubleArrayToVariant(JNIEnv* env, jdoubleArray array);

// Convert a Java array of objects into a Variant that holds a vector of
// whatever those objects are. Assumes that the objects are of a type that
// a variant can hold (i.e. trees of dictionaries and arrays whose leaves are
// all primitive types).
Variant JObjectArrayToVariant(JNIEnv* env, jobjectArray array);

// Convert a std::string into a jobject of type android.net.Uri.
// The caller must call env->DeleteLocalRef() on the returned jobject.
jobject CharsToJniUri(JNIEnv* env, const char* uri);

// Parse a string containing a URL into a android.net.Uri using Uri.parse().
// The caller must call env->DeleteLocalRef() on the returned jobject.
jobject ParseUriString(JNIEnv* env, const char* uri_string);

// Convert a jbyteArray to a vector, releasing the reference to the jbyteArray.
std::vector<unsigned char> JniByteArrayToVector(JNIEnv* env, jobject array);

// Convert a byte array with size into a Java byte[] (jbyteArray in JNI).
// The caller must call env->DeleteLocalRef() on the returned object.
jbyteArray ByteBufferToJavaByteArray(JNIEnv* env, const uint8_t* data,
                                     size_t size);

// Convert a local to global reference, deleting the specified local reference.
jobject LocalToGlobalReference(JNIEnv* env, jobject local_reference);

// Convenience function for using the Java builder pattern via JNI.
//
// Simply releases the old builder object and returns the new builder object.
// This allows you to use the builder pattern in JNI.
//
// For example:
// jobject builder = env->NewObject(SOME_CLASS, CONSTRUCTOR_METHOD);
// builder = ContinueBuilder(env, builder,
//                           env->CallObjectMethod(builder, SETTER_METHOD_1,
//                           ...));
// builder = ContinueBuilder(env, builder,
//                           env->CallObjectMethod(builder, SETTER_METHOD_2,
//                           ...));
// [ ...etc., until... ]
// jobject my_object = env->CallObjectMethod(builder, FINISH_BUILDING);
// env->DeleteLocalEnv(builder);
jobject ContinueBuilder(JNIEnv* env, jobject old_builder, jobject new_builder);

// Indicates how the Future was finished.
enum FutureResult {
  kFutureResultSuccess,    // The task finished successfully.
  kFutureResultFailure,    // The task encountered an error.
  kFutureResultCancelled,  // The task was cancelled before it finished.
};

// This callback function is called when `task` in
// RegisterCallbackOnTask() is completed.
// It will most likely be called from a different thread, so the caller must
// implement suitable thread synchronization.
//
// @param env The JNI environment. Fields of `result` can be read with this.
// @param result The result referred to by `task` in the
// call to RegisterCallbackOnTask().
// @param result_code The result of the Future (Success, Failure, Cancelled).
// @param callback_data Passed through verbatim from
// RegisterCallbackOnTask().
typedef void TaskCallbackFn(JNIEnv* env, jobject result,
                            FutureResult result_code,
                            const char* status_message, void* callback_data);

// Calls callback_fn when the Task `task` is complete, where `task` is an
// instance of com.google.android.gms.tasks.Task.
// NOTE: api_identifier must *not* be deallocated or can be nullptr to place
// the callback in the global pool.
void RegisterCallbackOnTask(JNIEnv* env, jobject task,
                            TaskCallbackFn callback_fn, void* callback_data,
                            const char* api_identifier);

// Cancel all callbacks associated with the specified API identifier.  If an
// API identifier isn't specified, all pending callbacks are cancelled.
void CancelCallbacks(JNIEnv* env, const char* api_identifier);

// Find a class and retrieve a global reference to it.
// If a set of files is provided and the class isn't found in the default class
// path, the files will be searched for the class and loaded.
// NOTE: This method will log an error if the class isn't found unless
// optional == kClassOptional.
jclass FindClassGlobal(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>* embedded_files,
    const char* class_name, ClassRequirement optional);

// Find a class, attempting to load the class if it's not found.
jclass FindClass(JNIEnv* env, const char* class_name);
// Deprecated method.
static inline jclass FindClass(JNIEnv* env, jobject activity_object,
                               const char* class_name) {
  return FindClass(env, class_name);
}

// Cache a list of embedded files to the activity's cache directory.
const std::vector<internal::EmbeddedFile>& CacheEmbeddedFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files);

// Attempt to load a class from a set of files which have been cached to local
// storage using CacheEmbeddedFiles().
jclass FindClassInFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files,
    const char* class_name);

// Get a resource ID from the activity's package.
int GetResourceIdFromActivity(JNIEnv* env, jobject activity_object,
                              const char* resource_name,
                              ResourceType resource_type);

// Get a resource value as a string from the activity's package.
std::string GetResourceStringFromActivity(JNIEnv* env, jobject activity_object,
                                          int resource_id);

// Get the name of the package associated with this activity.
std::string GetPackageName(JNIEnv* env, jobject activity_object);

// Run a C++ function on the main/UI thread.
// Be very careful using this function - make sure you clean up any JNI local
// object references you create in your callback during the function, as
// their deletion cannot occur in any other thread.
void RunOnMainThread(JNIEnv* env, jobject activity_object,
                     JavaThreadContext::Callback function_ptr,
                     void* function_data,
                     JavaThreadContext::Callback cancel_function_ptr = nullptr,
                     JavaThreadContext* context = nullptr);

// Run a C++ function on a background thread.
// Be very careful using this function - make sure you clean up any JNI local
// object references you create in your callback during the function, as
// their deletion cannot occur in any other thread.
void RunOnBackgroundThread(
    JNIEnv* env, JavaThreadContext::Callback function_ptr, void* function_data,
    JavaThreadContext::Callback cancel_function_ptr = nullptr,
    JavaThreadContext* context = nullptr);

// Check for JNI exceptions, report them if any, and clear them. Returns true
// if there was an exception, false if not.
bool CheckAndClearJniExceptions(JNIEnv* env);

// If an JNI exception occurred, this function will return the message
// and clear the exception.
// Otherwise, it will return an empty string.
std::string GetAndClearExceptionMessage(JNIEnv* env);

// Returns the message from an exception.
// This does not clear the JNI exception state.
std::string GetMessageFromException(JNIEnv* env, jobject exception);

// If an exception occurred, log its message with the given log level and an
// optional log message. If you specify a log_fmt, the given log message will
// displayed if an exception occurred, followed by a colon and then the
// exception's localized message. If you don't specify a log_fmt, the exception
// message will be displayed by itself.
//
// If an exception occurred, this function will clear it and return true.
// Otherwise it will return false.
bool LogException(JNIEnv* env, LogLevel log_level = kLogLevelError,
                  const char* log_fmt = nullptr, ...);

// Returns a pointer to the JNI environment.  Also, registers a destructor
// to automatically detach the thread from the JVM when it exits.  (Failure
// to detach threads will cause errors when exiting otherwise.)
JNIEnv* GetThreadsafeJNIEnv(JavaVM* java_vm);

// Attaches the current thread to a Java VM. Returns a JNI interface pointer in
// the JNIEnv argument. This deals with the different parameter requirements in
// JNI call, where NDK asks for JNIEnv** while JDK asks for void**.
//
// Users should use GetThreadsafeJNIEnv() instead to ensure they detach the JNI
// environment from the thread when exiting.
jint AttachCurrentThread(JavaVM* java_vm, JNIEnv** env);

// Returns a pointer to the JNI environment. This retrieves the JNIEnv from
// FIREBASE_NAMESPACE::App, either the default App (if it exists) or any valid
// App. If there is no instantiated App, returns nullptr.
JNIEnv* GetJNIEnvFromApp();

}  // namespace util
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_UTIL_ANDROID_H_
