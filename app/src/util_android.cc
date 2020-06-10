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

#include "app/src/util_android.h"

#include <assert.h>
#include <jni.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "app/app_resources.h"
#include "app/src/app_common.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Log method in log_android.h.
extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_app_internal_cpp_Log_nativeLog(
    JNIEnv* env, jobject instance, jint priority, jstring tag, jstring msg);

namespace util {

// clang-format off
#define CLASS_LOADER_METHODS(X) \
  X(LoadClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"), \
  X(FindLoadedClass, "findLoadedClass",                               \
    "(Ljava/lang/String;)Ljava/lang/Class;")
// clang-format on
METHOD_LOOKUP_DECLARATION(class_loader, CLASS_LOADER_METHODS)
METHOD_LOOKUP_DEFINITION(class_loader, "java/lang/ClassLoader",
                         CLASS_LOADER_METHODS)

// Used to setup the cache of Set class method IDs to reduce time spent looking
// up methods by string.
// clang-format off
#define JNI_RESULT_CALLBACK_METHODS(X) \
  X(Constructor, "<init>", "(Lcom/google/android/gms/tasks/Task;JJ)V"), \
  X(Cancel, "cancel", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(jniresultcallback, JNI_RESULT_CALLBACK_METHODS)
METHOD_LOOKUP_DEFINITION(
    jniresultcallback, "com/google/firebase/app/internal/cpp/JniResultCallback",
    JNI_RESULT_CALLBACK_METHODS)

// clang-format off
#define CPP_THREAD_DISPATCHER_CONTEXT_METHODS(X) \
  X(Constructor, "<init>", "(JJJ)V"),                                          \
  X(Cancel, "cancel", "()V"),                                                  \
  X(ReleaseExecuteCancelLock, "releaseExecuteCancelLock", "()V"),              \
  X(AcquireExecuteCancelLock, "acquireExecuteCancelLock", "()Z")
// clang-format on
METHOD_LOOKUP_DECLARATION(cppthreaddispatchercontext,
                          CPP_THREAD_DISPATCHER_CONTEXT_METHODS)
METHOD_LOOKUP_DEFINITION(
    cppthreaddispatchercontext,
    "com/google/firebase/app/internal/cpp/CppThreadDispatcherContext",
    CPP_THREAD_DISPATCHER_CONTEXT_METHODS)

// clang-format off
#define CPP_THREAD_DISPATCHER_METHODS(X) \
  X(RunOnMainThread, "runOnMainThread", "(Landroid/app/Activity;"              \
    "Lcom/google/firebase/app/internal/cpp/CppThreadDispatcherContext;)V",     \
    kMethodTypeStatic),                                                        \
  X(RunOnBackgroundThread, "runOnBackgroundThread",                            \
    "(Lcom/google/firebase/app/internal/cpp/CppThreadDispatcherContext;)V",    \
    kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(cppthreaddispatcher, CPP_THREAD_DISPATCHER_METHODS)
METHOD_LOOKUP_DEFINITION(
    cppthreaddispatcher,
    "com/google/firebase/app/internal/cpp/CppThreadDispatcher",
    CPP_THREAD_DISPATCHER_METHODS)

// clang-format off
#define LOG_METHODS(X)                                                         \
  X(Shutdown, "shutdown", "()V", util::kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(log, LOG_METHODS)
METHOD_LOOKUP_DEFINITION(log, "com/google/firebase/app/internal/cpp/Log",
                         LOG_METHODS)

// Methods of the android.net.Uri.Builder class,
// used to create android.net.Uri objects.
// clang-format off
#define URI_BUILDER_METHODS(X)                                                 \
    X(Constructor, "<init>", "()V"),                                           \
    X(EncodedPath, "encodedPath",                                              \
      "(Ljava/lang/String;)Landroid/net/Uri$Builder;"),                        \
    X(Build, "build", "()Landroid/net/Uri;")
// clang-format on
METHOD_LOOKUP_DECLARATION(uribuilder, URI_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(uribuilder, "android/net/Uri$Builder",
                         URI_BUILDER_METHODS)

// clang-format off
#define FILE_OUTPUT_STREAM_METHODS(X)                \
  X(ConstructorFile, "<init>", "(Ljava/io/File;)V"), \
  X(Write, "write", "([BII)V"),                      \
  X(Close, "close", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(file_output_stream, FILE_OUTPUT_STREAM_METHODS)
METHOD_LOOKUP_DEFINITION(file_output_stream, "java/io/FileOutputStream",
                         FILE_OUTPUT_STREAM_METHODS)

// clang-format off
#define DEX_CLASS_LOADER_METHODS(X) \
  X(Constructor, "<init>", \
    "(Ljava/lang/String;Ljava/lang/String;" \
    "Ljava/lang/String;Ljava/lang/ClassLoader;)V"), \
  X(LoadClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;")
// clang-format on
METHOD_LOOKUP_DECLARATION(dex_class_loader, DEX_CLASS_LOADER_METHODS)
METHOD_LOOKUP_DEFINITION(dex_class_loader,
                         PROGUARD_KEEP_CLASS "dalvik/system/DexClassLoader",
                         DEX_CLASS_LOADER_METHODS)

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)

// clang-format off
#define URL_CLASS_LOADER_METHODS(X) \
  X(Constructor, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V"), \
  X(LoadClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;")
// clang-format on
METHOD_LOOKUP_DECLARATION(url_class_loader, URL_CLASS_LOADER_METHODS)
METHOD_LOOKUP_DEFINITION(url_class_loader, "java/net/URLClassLoader",
                         URL_CLASS_LOADER_METHODS)

// clang-format off
#define URL_METHODS(X) \
  X(Constructor, "<init>", "(Ljava/net/URL;Ljava/lang/String;)V")
// clang-format on
METHOD_LOOKUP_DECLARATION(url, URL_METHODS)
METHOD_LOOKUP_DEFINITION(url, "java/net/URL", URL_METHODS)

// clang-format off
#define JAVA_URI_METHODS(X) X(ToUrl, "toURL", "()Ljava/net/URL;")
// clang-format on
METHOD_LOOKUP_DECLARATION(java_uri, JAVA_URI_METHODS)
METHOD_LOOKUP_DEFINITION(java_uri, "java/net/URI", JAVA_URI_METHODS)

#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

// Used to call android.content.res.Resources methods.
// clang-format off
#define RESOURCES_METHODS(X)                                         \
  X(GetIdentifier, "getIdentifier",                                  \
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I")
// clang-format on
METHOD_LOOKUP_DECLARATION(resources, RESOURCES_METHODS)
METHOD_LOOKUP_DEFINITION(resources, "android/content/res/Resources",
                         RESOURCES_METHODS)

METHOD_LOOKUP_DEFINITION(activity, "android/app/Activity", ACTIVITY_METHODS)
METHOD_LOOKUP_DEFINITION(array_list, "java/util/ArrayList", ARRAY_LIST_METHODS)
METHOD_LOOKUP_DEFINITION(asset_file_descriptor,
                         "android/content/res/AssetFileDescriptor",
                         ASSETFILEDESCRIPTOR_METHODS)
METHOD_LOOKUP_DEFINITION(boolean_class, "java/lang/Boolean", BOOLEAN_METHODS)
METHOD_LOOKUP_DEFINITION(bundle, "android/os/Bundle", BUNDLE_METHODS)
METHOD_LOOKUP_DEFINITION(byte_class, "java/lang/Byte", BYTE_METHODS)
METHOD_LOOKUP_DEFINITION(character_class, "java/lang/Character",
                         CHARACTER_METHODS);
METHOD_LOOKUP_DEFINITION(class_class, "java/lang/Class", CLASS_METHODS)
METHOD_LOOKUP_DEFINITION(content_resolver, "android/content/ContentResolver",
                         CONTENTRESOLVER_METHODS)
METHOD_LOOKUP_DEFINITION(context, "android/content/Context", CONTEXT_METHODS)
METHOD_LOOKUP_DEFINITION(cursor, "android/database/Cursor", CURSOR_METHODS)
METHOD_LOOKUP_DEFINITION(date, "java/util/Date", DATE_METHODS);
METHOD_LOOKUP_DEFINITION(double_class, "java/lang/Double", DOUBLE_METHODS)
METHOD_LOOKUP_DEFINITION(file, "java/io/File", FILE_METHODS)
METHOD_LOOKUP_DEFINITION(float_class, "java/lang/Float", FLOAT_METHODS)
METHOD_LOOKUP_DEFINITION(hash_map, "java/util/HashMap", HASHMAP_METHODS)
METHOD_LOOKUP_DEFINITION(integer_class, "java/lang/Integer", INTEGER_METHODS)
METHOD_LOOKUP_DEFINITION(intent, "android/content/Intent", INTENT_METHODS)
METHOD_LOOKUP_DEFINITION(iterable, "java/lang/Iterable", ITERABLE_METHODS)
METHOD_LOOKUP_DEFINITION(iterator, "java/util/Iterator", ITERATOR_METHODS)
METHOD_LOOKUP_DEFINITION(list, "java/util/List", LIST_METHODS)
METHOD_LOOKUP_DEFINITION(long_class, "java/lang/Long", LONG_METHODS)
METHOD_LOOKUP_DEFINITION(map, "java/util/Map", MAP_METHODS)
METHOD_LOOKUP_DEFINITION(parcel_file_descriptor,
                         "android/os/ParcelFileDescriptor",
                         PARCELFILEDESCRIPTOR_METHODS)
METHOD_LOOKUP_DEFINITION(set, "java/util/Set", SET_METHODS)
METHOD_LOOKUP_DEFINITION(short_class, "java/lang/Short", SHORT_METHODS);
METHOD_LOOKUP_DEFINITION(string, "java/lang/String", STRING_METHODS)
METHOD_LOOKUP_DEFINITION(throwable, "java/lang/Throwable", THROWABLE_METHODS)
METHOD_LOOKUP_DEFINITION(uri, "android/net/Uri", URI_METHODS)
METHOD_LOOKUP_DEFINITION(object, "java/lang/Object", OBJECT_METHODS)

// Number of references to this module via InitializeActivityClasses() vs.
// TerminateActivityClasses(). Note that the first instance of Initialize()
// also calls InitializeActivityClasses().
static unsigned int g_initialized_activity_count = 0;
// Number of references to this module via Initialize() vs. Terminate().
static unsigned int g_initialized_count = 0;

// Data associated with each Java callback in flight.
struct CallbackData {
  // Global reference to the Java callback class that references this
  // struct.
  jobject callback_reference;
  // User specified data for the callback.
  void* data;
  // References CallbackData within the list.
  std::list<CallbackData>::iterator iterator;
  // Reference to the list containing this structure.
  std::list<CallbackData>* list;
  // Whether the callback is complete.
  bool complete;
};
// Tracks the set of global references to Java callback classes in flight.
// This makes it possible to to remove references to the C++ objects when
// util::Terminate() is called.
static std::map<const char*, std::list<CallbackData>>* g_task_callbacks;
// Mutex for g_task_callbacks.
static pthread_mutex_t g_task_callbacks_mutex;

// Global references to class loaders used to find classes and load embedded
// classes.
static std::vector<jobject>* g_class_loaders;

JNIEXPORT void JNICALL JniResultCallback_nativeOnResult(
    JNIEnv* env, jobject clazz, jobject result, jboolean success,
    jboolean cancelled, jstring status_message, jlong callback_fn_param,
    jlong callback_data);

static const JNINativeMethod kJniCallbackMethod = {
    "nativeOnResult", "(Ljava/lang/Object;ZZLjava/lang/String;JJ)V",
    reinterpret_cast<void*>(JniResultCallback_nativeOnResult)};

static const JNINativeMethod kNativeLogMethods[] = {
    {"nativeLog", "(ILjava/lang/String;Ljava/lang/String;)V",
     reinterpret_cast<void*>(
         FIREBASE_NAMESPACE::
             Java_com_google_firebase_app_internal_cpp_Log_nativeLog)}};

static const char* kResourceTypeStrings[] = {
    RESOURCE_TYPES(RESOURCE_TYPE_STRING),
};

// LINT.IfChange
static const char kMissingJavaClassError[] =
    "Java class %s not found.  "
    "Please verify the AAR which contains the %s class is included "
    "in your app.";

static const char kMissingJavaMethodFieldError[] =
    "Unable to find %s.  "
    "Please verify the AAR which contains the %s class is included "
    "in your app.";
// LINT.ThenChange(//depot_firebase_cpp/app/client/unity/src/swig/app.SWIG)

// Create a global reference to the specified class loader add it to the
// list
// of class loaders and release the local reference.
static void AddClassLoader(JNIEnv* env, jobject class_loader_object) {
  assert(class_loader_object);
  assert(g_class_loaders);
  g_class_loaders->push_back(env->NewGlobalRef(class_loader_object));
  env->DeleteLocalRef(class_loader_object);
}

// Get the last class loader added to the g_class_loaders list.
static jobject GetParentLoader() {
  assert(g_class_loaders);
  return g_class_loaders->back();
}

// Try to find a class in the list of class loaders.
static jclass FindOrLoadClassFromLoaders(JNIEnv* env, const char* class_name) {
  assert(g_class_loaders);
  static const class_loader::Method kFindLoadClassMethods[] = {
      class_loader::kFindLoadedClass, class_loader::kLoadClass};
  jstring class_name_object = env->NewStringUTF(class_name);
  jclass class_object = nullptr;
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(kFindLoadClassMethods); ++i) {
    for (auto it = g_class_loaders->begin();
         !class_object && it != g_class_loaders->end(); ++it) {
      class_object = static_cast<jclass>(env->CallObjectMethod(
          *it, class_loader::GetMethodId(kFindLoadClassMethods[i]),
          class_name_object));
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        class_object = nullptr;
      }
    }
  }
  env->DeleteLocalRef(class_name_object);
  return class_object;
}

// Create the list of class loaders initializing it with the activity's
// class loader.
static void InitializeClassLoaders(JNIEnv* env, jobject activity_object) {
  assert(!g_class_loaders);
  g_class_loaders = new std::vector<jobject>();
  jobject class_loader = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetClassLoader));
  if (!CheckAndClearJniExceptions(env)) {
    AddClassLoader(env, class_loader);
  }
}

// Remove all global references to class loaders and destroy the list.
static void TerminateClassLoaders(JNIEnv* env) {
  assert(g_class_loaders);
  for (auto it = g_class_loaders->begin(); it != g_class_loaders->end(); ++it) {
    env->DeleteGlobalRef(*it);
  }
  delete g_class_loaders;
  g_class_loaders = nullptr;
}

// This class executes the callback when it goes out of scope, unless Cancel()
// is called first.
template <typename T>
class ScopedCleanup {
 public:
  // cb is a callback to be called when the instance goes out of scope.
  // user_data here will be passed to the callback.
  ScopedCleanup(void (*callback)(T*), T* user_data)
      : callback_(callback), user_data_(user_data) {}

  ~ScopedCleanup() {
    if (callback_) callback_(user_data_);
  }

  void Cancel() { callback_ = nullptr; }

 private:
  void (*callback_)(T*);
  T* user_data_;
};

// Release cached classes.
static void ReleaseClasses(JNIEnv* env) {
  asset_file_descriptor::ReleaseClass(env);
  array_list::ReleaseClass(env);
  boolean_class::ReleaseClass(env);
  bundle::ReleaseClass(env);
  byte_class::ReleaseClass(env);
  character_class::ReleaseClass(env);
  class_class::ReleaseClass(env);
  content_resolver::ReleaseClass(env);
  context::ReleaseClass(env);
  cursor::ReleaseClass(env);
  date::ReleaseClass(env);
  dex_class_loader::ReleaseClass(env);
  double_class::ReleaseClass(env);
  file::ReleaseClass(env);
  file_output_stream::ReleaseClass(env);
  float_class::ReleaseClass(env);
  hash_map::ReleaseClass(env);
  integer_class::ReleaseClass(env);
  intent::ReleaseClass(env);
  iterable::ReleaseClass(env);
  iterator::ReleaseClass(env);
  log::ReleaseClass(env);
  long_class::ReleaseClass(env);
  list::ReleaseClass(env);
  map::ReleaseClass(env);
  parcel_file_descriptor::ReleaseClass(env);
  resources::ReleaseClass(env);
  set::ReleaseClass(env);
  short_class::ReleaseClass(env);
  string::ReleaseClass(env);
  throwable::ReleaseClass(env);
  uri::ReleaseClass(env);
  object::ReleaseClass(env);
  uribuilder::ReleaseClass(env);
  jniresultcallback::ReleaseClass(env);
  JavaThreadContext::Terminate(env);
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  java_uri::ReleaseClass(env);
  url::ReleaseClass(env);
  url_class_loader::ReleaseClass(env);
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
}

// Releases only the classes from InitializeActivityClasses.
// This is defined just for pairity with ReleaseClasses above, in the context
// of classes initialized with InitializeActivityClasses.
static void ReleaseActivityClasses(JNIEnv* env) {
  activity::ReleaseClass(env);
  class_loader::ReleaseClass(env);
}

bool InitializeActivityClasses(JNIEnv* env, jobject activity_object) {
  g_initialized_activity_count++;
  if (g_initialized_activity_count > 1) {
    return true;
  }

  ScopedCleanup<JNIEnv> cleanup(TerminateActivityClasses, env);

  // If we get here, it's the first call to Initialize.
  // Cache method pointers.
  if (!(activity::CacheMethodIds(env, activity_object) &&
        class_loader::CacheMethodIds(env, activity_object))) {
    return false;
  }
  InitializeClassLoaders(env, activity_object);
  CheckAndClearJniExceptions(env);

  cleanup.Cancel();
  return true;
}

void TerminateActivityClasses(JNIEnv* env) {
  FIREBASE_ASSERT(g_initialized_activity_count);
  g_initialized_activity_count--;
  if (g_initialized_activity_count == 0) {
    ReleaseActivityClasses(env);
    if (g_class_loaders) {
      TerminateClassLoaders(env);
    }
  }
}

bool Initialize(JNIEnv* env, jobject activity_object) {
  // Note: We increment g_initialized_count when it's possible to call
  // Terminate() in all clean up code paths. ScopedCleanup calls Terminate() to
  // clean up when its destructor is called, decrementing g_initialized_count.
  if (g_initialized_count != 0) {
    g_initialized_count++;
    return true;
  }

  if (!InitializeActivityClasses(env, activity_object)) {
    return false;
  }

  // Cache method pointers.
  if (!(array_list::CacheMethodIds(env, activity_object) &&
        asset_file_descriptor::CacheMethodIds(env, activity_object) &&
        boolean_class::CacheMethodIds(env, activity_object) &&
        bundle::CacheMethodIds(env, activity_object) &&
        byte_class::CacheMethodIds(env, activity_object) &&
        character_class::CacheMethodIds(env, activity_object) &&
        class_class::CacheMethodIds(env, activity_object) &&
        content_resolver::CacheMethodIds(env, activity_object) &&
        context::CacheMethodIds(env, activity_object) &&
        cursor::CacheMethodIds(env, activity_object) &&
        date::CacheMethodIds(env, activity_object) &&
        dex_class_loader::CacheMethodIds(env, activity_object) &&
        double_class::CacheMethodIds(env, activity_object) &&
        file::CacheMethodIds(env, activity_object) &&
        file_output_stream::CacheMethodIds(env, activity_object) &&
        float_class::CacheMethodIds(env, activity_object) &&
        hash_map::CacheMethodIds(env, activity_object) &&
        integer_class::CacheMethodIds(env, activity_object) &&
        intent::CacheMethodIds(env, activity_object) &&
        iterable::CacheMethodIds(env, activity_object) &&
        iterator::CacheMethodIds(env, activity_object) &&
        list::CacheMethodIds(env, activity_object) &&
        long_class::CacheMethodIds(env, activity_object) &&
        map::CacheMethodIds(env, activity_object) &&
        parcel_file_descriptor::CacheMethodIds(env, activity_object) &&
        resources::CacheMethodIds(env, activity_object) &&
        set::CacheMethodIds(env, activity_object) &&
        short_class::CacheMethodIds(env, activity_object) &&
        string::CacheMethodIds(env, activity_object) &&
        throwable::CacheMethodIds(env, activity_object) &&
        uri::CacheMethodIds(env, activity_object) &&
        object::CacheMethodIds(env, activity_object) &&
        uribuilder::CacheMethodIds(env, activity_object))) {
    ReleaseClasses(env);
    TerminateActivityClasses(env);
    return false;
  }

  ScopedCleanup<JNIEnv> cleanup(Terminate, env);
  // If anything returns early from here on, it will run Terminate to clean up
  // via the ScopedCleanup destructor. That will decrement the reference count,
  // so we need to bump the ref count now.
  g_initialized_count++;

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  // Cache JVM class-loader for desktop.
  if (!(java_uri::CacheMethodIds(env, activity_object) &&
        url::CacheMethodIds(env, activity_object) &&
        url_class_loader::CacheMethodIds(env, activity_object))) {
    return false;
  }
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

  // Cache embedded files and load embedded classes.
  const std::vector<internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(
          env, activity_object,
          internal::EmbeddedFile::ToVector(firebase_app::app_resources_filename,
                                           firebase_app::app_resources_data,
                                           firebase_app::app_resources_size));

  // Cache the Log class and register the native log method.
  if (!(log::CacheClassFromFiles(env, activity_object, &embedded_files) !=
            nullptr &&
        log::CacheMethodIds(env, activity_object) &&
        log::RegisterNatives(env, kNativeLogMethods,
                             FIREBASE_ARRAYSIZE(kNativeLogMethods)))) {
    return false;
  }

  if (!(jniresultcallback::CacheClassFromFiles(env, activity_object,
                                               &embedded_files) &&
        jniresultcallback::CacheMethodIds(env, activity_object) &&
        jniresultcallback::RegisterNatives(env, &kJniCallbackMethod, 1))) {
    return false;
  }

  if (!JavaThreadContext::Initialize(env, activity_object, embedded_files)) {
    return false;
  }
  CheckAndClearJniExceptions(env);

  g_task_callbacks = new std::map<const char*, std::list<CallbackData>>();
  g_task_callbacks_mutex = PTHREAD_MUTEX_INITIALIZER;
  cleanup.Cancel();
  return true;
}

// Terminate the utilities library.  Releases all global references to
// classes.
void Terminate(JNIEnv* env) {
  FIREBASE_ASSERT(g_initialized_count);
  g_initialized_count--;
  if (g_initialized_count == 0) {
    if (g_task_callbacks) {
      CancelCallbacks(env, nullptr);
      pthread_mutex_lock(&g_task_callbacks_mutex);
      delete g_task_callbacks;
      g_task_callbacks = nullptr;
      pthread_mutex_unlock(&g_task_callbacks_mutex);
      pthread_mutex_destroy(&g_task_callbacks_mutex);
    }

    // Shutdown the log if it was initialized.
    jclass log_class = log::GetClass();
    if (log_class) {
      env->CallStaticVoidMethod(log_class, log::GetMethodId(log::kShutdown));
      CheckAndClearJniExceptions(env);
    }

    ReleaseClasses(env);
    TerminateActivityClasses(env);
  }
}

bool LookupMethodIds(JNIEnv* env, jclass clazz,
                     const MethodNameSignature* method_name_signatures,
                     size_t number_of_method_name_signatures,
                     jmethodID* method_ids, const char* class_name) {
  FIREBASE_ASSERT(method_name_signatures);
  FIREBASE_ASSERT(number_of_method_name_signatures > 0);
  FIREBASE_ASSERT(method_ids);
  FIREBASE_ASSERT_MESSAGE_RETURN(false, clazz, kMissingJavaClassError,
                                 class_name, class_name);
  LogDebug("Looking up methods for %s", class_name);
  for (size_t i = 0; i < number_of_method_name_signatures; ++i) {
    const MethodNameSignature& method = method_name_signatures[i];
    if (method.optional == kMethodOptional && method.name == nullptr) {
      continue;
    }
    switch (method.type) {
      case kMethodTypeInstance:
        method_ids[i] = env->GetMethodID(clazz, method.name, method.signature);
        break;
      case kMethodTypeStatic:
        method_ids[i] =
            env->GetStaticMethodID(clazz, method.name, method.signature);
        break;
    }
    if (CheckAndClearJniExceptions(env)) {
      method_ids[i] = 0;
    }
    char method_message[256];
    snprintf(method_message, sizeof(method_message),
             "Method %s.%s (signature '%s', %s)", class_name, method.name,
             method.signature,
             method.type == kMethodTypeInstance ? "instance" : "static");
    LogDebug("%s (optional %d) 0x%08x%s", method_message,
             (method.optional == kMethodOptional) ? 1 : 0,
             static_cast<int>(reinterpret_cast<intptr_t>(method_ids[i])),
             method_ids[i] ? "" : " (not found)");
    FIREBASE_ASSERT_MESSAGE_RETURN(
        false, method_ids[i] || (method.optional == kMethodOptional),
        kMissingJavaMethodFieldError, method_message, class_name);
  }
  return true;
}

// Lookup field IDs specified by the field_descriptors array and store
// in field_ids.  Used by FIELD_LOOKUP_DECLARATION.
bool LookupFieldIds(JNIEnv* env, jclass clazz,
                    const FieldDescriptor* field_descriptors,
                    size_t number_of_field_descriptors, jfieldID* field_ids,
                    const char* class_name) {
  FIREBASE_ASSERT(field_descriptors);
  FIREBASE_ASSERT(number_of_field_descriptors > 0);
  FIREBASE_ASSERT(field_ids);
  FIREBASE_ASSERT_MESSAGE_RETURN(false, clazz, kMissingJavaClassError,
                                 class_name, class_name);
  LogDebug("Looking up fields for %s", class_name);
  for (size_t i = 0; i < number_of_field_descriptors; ++i) {
    const FieldDescriptor& field = field_descriptors[i];
    if (field.optional == kMethodOptional && field.name == nullptr) {
      continue;
    }
    switch (field.type) {
      case kFieldTypeInstance:
        field_ids[i] = env->GetFieldID(clazz, field.name, field.signature);
        break;
      case kFieldTypeStatic:
        field_ids[i] =
            env->GetStaticFieldID(clazz, field.name, field.signature);
        break;
    }
    if (CheckAndClearJniExceptions(env)) {
      field_ids[i] = 0;
    }
    char field_message[256];
    snprintf(field_message, sizeof(field_message),
             "Field %s.%s (signature '%s', %s)", class_name, field.name,
             field.signature,
             field.type == kFieldTypeInstance ? "instance" : "static");
    LogDebug("%s (optional %d) 0x%08x%s", field_message,
             (field.optional == kMethodOptional) ? 1 : 0,
             static_cast<int>(reinterpret_cast<intptr_t>(field_ids[i])),
             field_ids[i] ? "" : " (not found)");
    FIREBASE_ASSERT_MESSAGE_RETURN(
        false, field_ids[i] || (field.optional == kMethodOptional),
        kMissingJavaMethodFieldError, field_message, class_name);
  }
  return true;
}

// Converts a `std::vector<std::string>` to a `java.util.ArrayList<String>`
// Returns a local ref to a List.
jobject StdVectorToJavaList(JNIEnv* env,
                            const std::vector<std::string>& string_vector) {
  jobject java_list =
      env->NewObject(array_list::GetClass(),
                     array_list::GetMethodId(array_list::kConstructor));
  jmethodID add_method = array_list::GetMethodId(array_list::kAdd);
  for (auto it = string_vector.begin(); it != string_vector.end(); ++it) {
    jstring value = env->NewStringUTF(it->c_str());
    env->CallBooleanMethod(java_list, add_method, value);
    CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(value);
  }
  return java_list;
}

// Converts a `std::map<Variant, Variant>` to a `java.util.Map<Object, Object>`.
// Returns a local ref to a Map.
jobject VariantMapToJavaMap(JNIEnv* env,
                            const std::map<Variant, Variant>& variant_map) {
  jobject java_map = env->NewObject(
      hash_map::GetClass(), hash_map::GetMethodId(hash_map::kConstructor));
  jmethodID put_method = map::GetMethodId(map::kPut);
  for (auto it = variant_map.begin(); it != variant_map.end(); ++it) {
    jobject key = VariantToJavaObject(env, it->first);
    jobject value = VariantToJavaObject(env, it->second);
    jobject previous = env->CallObjectMethod(java_map, put_method, key, value);
    CheckAndClearJniExceptions(env);
    if (previous) env->DeleteLocalRef(previous);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(key);
  }
  return java_map;
}

// Converts an `std::map<const char*, const char*>` to a
// `java.util.Map<String, String>`.
void StdMapToJavaMap(JNIEnv* env, jobject* to,
                     const std::map<const char*, const char*>& string_map) {
  jmethodID put_method = map::GetMethodId(map::kPut);
  for (std::map<const char*, const char*>::const_iterator it =
           string_map.begin();
       it != string_map.end(); ++it) {
    jstring key = env->NewStringUTF(it->first);
    jstring value = env->NewStringUTF(it->second);
    jobject previous = env->CallObjectMethod(*to, put_method, key, value);
    CheckAndClearJniExceptions(env);
    if (previous) env->DeleteLocalRef(previous);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(key);
  }
}

// Converts an `std::map<std::string, std::string>` to a
// `java.util.Map<String, String>`.
void StdMapToJavaMap(JNIEnv* env, jobject* to,
                     const std::map<std::string, std::string>& from) {
  jmethodID put_method = map::GetMethodId(map::kPut);
  for (std::map<std::string, std::string>::const_iterator iter = from.begin();
       iter != from.end(); ++iter) {
    jstring key = env->NewStringUTF(iter->first.c_str());
    jstring value = env->NewStringUTF(iter->second.c_str());
    jobject previous = env->CallObjectMethod(*to, put_method, key, value);
    CheckAndClearJniExceptions(env);
    if (previous) env->DeleteLocalRef(previous);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(key);
  }
}

// Converts a `java.util.Map<String, String>` to an
// `std::map<std::string, std::string>`.
// T is the type of the map. Our current requirements always have the same Key
// and Value types, so just specify one type.
// ConvertFn is of type:  T Function(JNIEnv* env, jobject obj)
template <typename T, typename ConvertFn>
static void JavaMapToStdMapTemplate(JNIEnv* env, std::map<T, T>* to,
                                    jobject from, ConvertFn convert) {
  // Set<Object> key_set = from.keySet();
  jobject key_set = env->CallObjectMethod(from, map::GetMethodId(map::kKeySet));
  CheckAndClearJniExceptions(env);
  // Iterator iter = key_set.iterator();
  jobject iter =
      env->CallObjectMethod(key_set, set::GetMethodId(set::kIterator));
  CheckAndClearJniExceptions(env);
  // while (iter.hasNext())
  while (
      env->CallBooleanMethod(iter, iterator::GetMethodId(iterator::kHasNext))) {
    CheckAndClearJniExceptions(env);
    // T key = iter.next();
    // T value = from.get(key);
    jobject key_object =
        env->CallObjectMethod(iter, iterator::GetMethodId(iterator::kNext));
    CheckAndClearJniExceptions(env);
    jobject value_object =
        env->CallObjectMethod(from, map::GetMethodId(map::kGet), key_object);
    CheckAndClearJniExceptions(env);
    const T key = convert(env, key_object);
    const T value = convert(env, value_object);
    env->DeleteLocalRef(key_object);
    env->DeleteLocalRef(value_object);

    to->insert(std::pair<T, T>(key, value));
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}

// Converts a `java.util.Map<String, String>` to an
// `std::map<std::string, std::string>`.
void JavaMapToStdMap(JNIEnv* env, std::map<std::string, std::string>* to,
                     jobject from) {
  JavaMapToStdMapTemplate<std::string>(env, to, from, JStringToString);
}

// Converts a `java.util.Map<java.lang.Object, java.lang.Object>` to an
// `std::map<Variant, Variant>`.
void JavaMapToVariantMap(JNIEnv* env, std::map<Variant, Variant>* to,
                         jobject from) {
  JavaMapToStdMapTemplate<Variant>(env, to, from, JavaObjectToVariant);
}

// Converts a `java.util.Set<String>` to a `std::vector<std::string>`.
void JavaSetToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                              jobject from) {
  // Use an iterator over the set.
  jobject iter =
      env->CallObjectMethod(from, util::set::GetMethodId(util::set::kIterator));
  CheckAndClearJniExceptions(env);
  // while (iter.hasNext())
  while (env->CallBooleanMethod(
      iter, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    CheckAndClearJniExceptions(env);
    // String elem = iter.next();
    jobject elem_object = env->CallObjectMethod(
        iter, util::iterator::GetMethodId(util::iterator::kNext));
    CheckAndClearJniExceptions(env);
    std::string elem = JniStringToString(env, elem_object);
    to->push_back(elem);
  }
  env->DeleteLocalRef(iter);
}

// Converts a `std::vector<Variant>` to a `java.util.List<Object>`.
// Returns a local ref to a List.
jobject VariantVectorToJavaList(JNIEnv* env,
                                const std::vector<Variant>& variant_vector) {
  jobject java_list =
      env->NewObject(array_list::GetClass(),
                     array_list::GetMethodId(array_list::kConstructor));
  jmethodID add_method = array_list::GetMethodId(array_list::kAdd);
  for (auto it = variant_vector.begin(); it != variant_vector.end(); ++it) {
    jobject value = VariantToJavaObject(env, *it);
    env->CallBooleanMethod(java_list, add_method, value);
    CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(value);
  }
  return java_list;
}

// Converts a `java.util.List<String>` to a `std::vector<std::string>`.
void JavaListToStdStringVector(JNIEnv* env, std::vector<std::string>* vector,
                               jobject from) {
  int size = env->CallIntMethod(from, list::GetMethodId(list::kSize));
  CheckAndClearJniExceptions(env);
  vector->clear();
  vector->reserve(size);
  for (int i = 0; i < size; i++) {
    jobject element =
        env->CallObjectMethod(from, list::GetMethodId(list::kGet), i);
    CheckAndClearJniExceptions(env);
    vector->push_back(JniStringToString(env, element));
  }
}

// Converts a `java.util.List<Object>` to a `std::vector<std::string>`.
void JavaObjectListToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                                     jobject from) {
  int size = env->CallIntMethod(from, list::GetMethodId(list::kSize));
  CheckAndClearJniExceptions(env);
  to->clear();
  to->reserve(size);
  for (int i = 0; i < size; i++) {
    jobject element =
        env->CallObjectMethod(from, list::GetMethodId(list::kGet), i);
    CheckAndClearJniExceptions(env);
    to->push_back(JniObjectToString(env, element));
    env->DeleteLocalRef(element);
  }
}

// Converts a `java.util.List<java.lang.Object>` to a `std::vector<Variant>`.
void JavaListToVariantList(JNIEnv* env, std::vector<Variant>* to,
                           jobject from) {
  // int size = from.size();
  int size = env->CallIntMethod(from, list::GetMethodId(list::kSize));
  CheckAndClearJniExceptions(env);
  to->clear();
  to->reserve(size);

  for (int i = 0; i < size; i++) {
    // Object obj = from.get(i);
    jobject obj = env->CallObjectMethod(from, list::GetMethodId(list::kGet), i);
    CheckAndClearJniExceptions(env);

    to->push_back(JavaObjectToVariant(env, obj));
    env->DeleteLocalRef(obj);
  }
}

// Convert a jstring to a std::string, releasing the reference to the
// jstring.
std::string JniStringToString(JNIEnv* env, jobject string_object) {
  std::string return_string = JStringToString(env, string_object);
  env->DeleteLocalRef(string_object);
  return return_string;
}

// Convert a Java object of type java.lang.Object into an std::string, by
// calling toString(), then release the object.
std::string JniObjectToString(JNIEnv* env, jobject obj) {
  if (obj == nullptr) return "";
  jobject str =
      env->CallObjectMethod(obj, object::GetMethodId(object::kToString));
  CheckAndClearJniExceptions(env);
  return JniStringToString(env, str);
}

// Convert a jstring (created by the JVM e.g passed into a native method)
// into
// a std::string.  Unlike JniStringToString() this does not release the
// reference to the string_object as the caller owns the object in a native
// method.
std::string JStringToString(JNIEnv* env, jobject string_object) {
  if (string_object == nullptr) return "";
  const char* string_buffer =
      env->GetStringUTFChars(static_cast<jstring>(string_object), 0);
  std::string return_string(string_buffer);
  env->ReleaseStringUTFChars(static_cast<jstring>(string_object),
                             string_buffer);
  return return_string;
}

bool IsJArray(JNIEnv* env, jobject obj) {
  jclass obj_class = env->GetObjectClass(obj);
  const bool is_array = env->CallBooleanMethod(
      obj_class, class_class::GetMethodId(class_class::kIsArray));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(obj_class);
  return is_array;
}

std::string JObjectClassName(JNIEnv* env, jobject obj) {
  jclass obj_class = env->GetObjectClass(obj);
  jobject obj_name = env->CallObjectMethod(
      obj_class, class_class::GetMethodId(class_class::kGetName));
  CheckAndClearJniExceptions(env);
  return JniStringToString(env, obj_name);
}

// Allow macros to iterate through all the primitive JNI types.
#define FIREBASE_JNI_PRIMITIVE_TYPES(X)                       \
  X(boolean_class, "[Z", jboolean, Boolean, bool, Bool, "%d") \
  X(byte_class, "[B", jbyte, Byte, uint8_t, UInt8, "%d")      \
  X(character_class, "[C", jchar, Char, char, Char, "%c")     \
  X(short_class, "[S", jshort, Short, int16_t, Int16, "%d")   \
  X(integer_class, "[I", jint, Int, int, Int, "%d")           \
  X(long_class, "[J", jlong, Long, int64_t, Int64, "%ld")     \
  X(float_class, "[F", jfloat, Float, float, Float, "%f")     \
  X(double_class, "[D", jdouble, Double, double, Double, "%f")

// Defines functions like this, but for all primitive JNI types.
//
// bool JBooleanToBool(JNIEnv* env, jobject obj) {
//   return env->CallBooleanMethod(
//       obj, boolean_class::GetMethodId(boolean_class::kValue));
// }
#define FIREBASE_JTYPE_TO_TYPE(type_class, array_format, jtype, jname, ctype, \
                               cname, printf_type)                            \
  ctype J##jname##To##cname(JNIEnv* env, jobject obj) {                       \
    ctype ret = env->Call##jname##Method(                                     \
        obj, type_class::GetMethodId(type_class::kValue));                    \
    CheckAndClearJniExceptions(env);                                          \
    return ret;                                                               \
  }
FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_JTYPE_TO_TYPE)

// Defines functions like this, but for all primitive JNI types.
//
// bool IsJBooleanArray(JNIEnv* env, jobject obj) {
//   jclass array_class = env->FindClass("[Z");
//   const bool is_array = env->IsInstanceOf(obj, array_class);
//   env->DeleteLocalRef(array_class);
//   return is_array;
// }
#define FIREBASE_IS_JARRAY(type_class, array_format, jtype, jname, ctype, \
                           cname, printf_type)                            \
  bool IsJ##jname##Array(JNIEnv* env, jobject obj) {                      \
    jclass array_class = env->FindClass(array_format);                    \
    const bool is_array = env->IsInstanceOf(obj, array_class);            \
    env->DeleteLocalRef(array_class);                                     \
    return is_array;                                                      \
  }
FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_IS_JARRAY)

// Defines functions like this, but for all the JNI array types.
//
// Variant JBooleanArrayToVariant(JNIEnv* env, jbooleanArray array) {
//   const size_t len = env->GetArrayLength(array);
//   jboolean* c_array = env->GetBooleanArrayElements(array, nullptr);
//   // We can't use the Variant array constructor because we have to cast to
//   // the C-type for each element (e.g. jboolean is char, which will become
//   // int64_t in the Variant, but we want bool in the Variant).
//   auto vec = new std::vector<Variant>(len);
//   for (size_t i = 0; i < len; ++i) {
//     (*vec)[i] = Variant(static_cast<bool>(c_array[i]));
//   }
//   Variant v;
//   // Transfer ownership of vec to the Variant. Set vec to NULL.
//   v.AssignVector(&vec);
//   env->ReleaseBooleanArrayElements(array, c_array, JNI_ABORT);
//   return v;
// }
#define FIREBASE_JARRAY_TO_VARIANT(type_class, array_format, jtype, jname, \
                                   ctype, cname, printf_type)              \
  Variant J##jname##ArrayToVariant(JNIEnv* env, jtype##Array array) {      \
    const size_t len = env->GetArrayLength(array);                         \
    jtype* c_array = env->Get##jname##ArrayElements(array, nullptr);       \
    auto vec = new std::vector<Variant>(len);                              \
    for (size_t i = 0; i < len; ++i) {                                     \
      (*vec)[i] = Variant(static_cast<ctype>(c_array[i]));                 \
    }                                                                      \
    Variant v;                                                             \
    v.AssignVector(&vec);                                                  \
    env->Release##jname##ArrayElements(array, c_array, JNI_ABORT);         \
    return v;                                                              \
  }
FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_JARRAY_TO_VARIANT)

Variant JObjectArrayToVariant(JNIEnv* env, jobjectArray array) {
  const size_t len = env->GetArrayLength(array);
  auto vec = new std::vector<Variant>();
  vec->reserve(len);

  // Loop through array converted each object into a Variant.
  for (size_t i = 0; i < len; ++i) {
    jobject obj = env->GetObjectArrayElement(array, i);
    vec->push_back(JavaObjectToVariant(env, obj));
    env->DeleteLocalRef(obj);
  }

  // Move vec so that we don't have to make a copy of it.
  Variant v;
  v.AssignVector(&vec);
  return v;
}

// Code like this, but for all JNI primitive types.
//
//  if (IsJBooleanArray(env, array))
//    return JBooleanArrayToVariant(env, static_cast<jbooleanArray>(array));
#define FIREBASE_CONVERT_TO_VARIANT_ARRAY(type_class, array_format, jtype,  \
                                          jname, ctype, cname, printf_type) \
  if (IsJ##jname##Array(env, array)) {                                      \
    return J##jname##ArrayToVariant(env, static_cast<jtype##Array>(array)); \
  }

Variant JArrayToVariant(JNIEnv* env, jarray array) {
  FIREBASE_ASSERT(IsJArray(env, array));

  // Check each array type in turn, and convert to it if the type matches.
  FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_CONVERT_TO_VARIANT_ARRAY)

  // Must be an array of objects. Convert each object independently.
  return JObjectArrayToVariant(env, static_cast<jobjectArray>(array));
}

// Code like this, but for all JNI primitive types.
//
//  if (env->IsInstanceOf(object, boolean_class::GetClass())) {  \
//    return Variant(JBooleanToBool(env, object));
//  }
#define FIREBASE_CONVERT_TO_VARIANT(type_class, array_format, jtype, jname, \
                                    ctype, cname, printf_type)              \
  if (env->IsInstanceOf(object, type_class::GetClass())) {                  \
    return Variant(J##jname##To##cname(env, object));                       \
  }

Variant JavaObjectToVariant(JNIEnv* env, jobject object) {
  if (object == nullptr) return Variant();

  // Convert strings.
  if (env->IsInstanceOf(object, string::GetClass()))
    return Variant(JStringToString(env, object));

  // Convert Dates to millis since epoch.
  if (env->IsInstanceOf(object, date::GetClass())) {
    auto getTime = date::GetMethodId(date::kGetTime);
    jlong millis = env->CallLongMethod(object, getTime);
    CheckAndClearJniExceptions(env);
    return Variant(static_cast<int64_t>(millis));
  }

  // Convert other primitive types.
  FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_CONVERT_TO_VARIANT)

  // Convert maps.
  if (env->IsInstanceOf(object, map::GetClass())) {
    Variant v;
    auto c_map = new std::map<Variant, Variant>();
    JavaMapToVariantMap(env, c_map, object);
    v.AssignMap(&c_map);
    return v;
  }

  // Convert lists.
  if (env->IsInstanceOf(object, list::GetClass())) {
    Variant v;
    auto c_vector = new std::vector<Variant>();
    JavaListToVariantList(env, c_vector, object);
    v.AssignVector(&c_vector);
    return v;
  }

  // Convert arrays.
  if (IsJArray(env, object)) {
    return JArrayToVariant(env, static_cast<jarray>(object));
  }

  // Unsupported type.
  LogWarning("Class %s cannot be converted to Variant, leaving empty.",
             JObjectClassName(env, object).c_str());
  return Variant();
}

jobject VariantToJavaObject(JNIEnv* env, const Variant& variant) {
  if (variant.is_null()) {
    return nullptr;
  } else if (variant.is_int64()) {
    return env->NewObject(long_class::GetClass(),
                          long_class::GetMethodId(long_class::kConstructor),
                          variant.int64_value());
  } else if (variant.is_double()) {
    return env->NewObject(double_class::GetClass(),
                          double_class::GetMethodId(double_class::kConstructor),
                          variant.double_value());
  } else if (variant.is_bool()) {
    return env->NewObject(
        boolean_class::GetClass(),
        boolean_class::GetMethodId(boolean_class::kConstructor),
        variant.bool_value());
  } else if (variant.is_string()) {
    return env->NewStringUTF(variant.string_value());
  } else if (variant.is_blob()) {
    return static_cast<jobject>(ByteBufferToJavaByteArray(
        env, variant.blob_data(), variant.blob_size()));
  } else if (variant.is_vector()) {
    return VariantVectorToJavaList(env, variant.vector());
  } else if (variant.is_map()) {
    return VariantMapToJavaMap(env, variant.map());
  } else {
    // Unsupported type.
    LogWarning("Variant cannot be converted to Java Object, returning null.");
    return nullptr;
  }
}

// Convert a jobject of type android.net.Uri into an std::string,
// and releases the reference to the jobject.
std::string JniUriToString(JNIEnv* env, jobject uri) {
  if (uri == nullptr) return "";
  jobject path = env->CallObjectMethod(uri, uri::GetMethodId(uri::kToString));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(uri);
  return JniStringToString(env, path);
}

// Convert a std::string into a jobject of type android.net.Uri.
// The caller must call env->DeleteLocalRef() on the returned jobject.
jobject CharsToJniUri(JNIEnv* env, const char* uri) {
  jobject uri_object = nullptr;
  // Create a new android.net.Uri.Builder.
  jobject builder =
      env->NewObject(uribuilder::GetClass(),
                     uribuilder::GetMethodId(uribuilder::kConstructor));

  // Call builder.path().
  jstring uri_string = env->NewStringUTF(uri);
  jobject builder_discard = env->CallObjectMethod(
      builder, uribuilder::GetMethodId(uribuilder::kEncodedPath), uri_string);
  if (!CheckAndClearJniExceptions(env)) {
    // Call builder.build().
    uri_object = env->CallObjectMethod(
        builder, uribuilder::GetMethodId(uribuilder::kBuild));
    // Release all locally created objects.
    env->DeleteLocalRef(builder_discard);
  }
  env->DeleteLocalRef(uri_string);
  env->DeleteLocalRef(builder);

  // Return the jobject. The caller is responsible for calling
  // DeleteLocalRef().
  return uri_object;
}

// Parse a string containing a URL into a android.net.Uri using Uri.parse().
// The caller must call env->DeleteLocalRef() on the returned jobject.
jobject ParseUriString(JNIEnv* env, const char* uri_string) {
  jobject path_str = env->NewStringUTF(uri_string);
  jobject uri = env->CallStaticObjectMethod(
      uri::GetClass(), uri::GetMethodId(uri::kParse), path_str);
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(path_str);
  return uri;
}

// Convert a jbyteArray to a vector, releasing the reference to the
// jbyteArray.
std::vector<unsigned char> JniByteArrayToVector(JNIEnv* env, jobject array) {
  std::vector<unsigned char> value;
  jbyteArray byte_array = static_cast<jbyteArray>(array);
  jsize byte_array_length = env->GetArrayLength(byte_array);
  if (byte_array_length) {
    value.resize(byte_array_length);
    env->GetByteArrayRegion(byte_array, 0, byte_array_length,
                            reinterpret_cast<jbyte*>(&value[0]));
  }
  env->DeleteLocalRef(array);
  return value;
}

// Convert a byte buffer and size into a jbyteArray.
jbyteArray ByteBufferToJavaByteArray(JNIEnv* env, const uint8_t* data,
                                     size_t size) {
  jbyteArray output_array = env->NewByteArray(size);
  env->SetByteArrayRegion(output_array, 0, size,
                          reinterpret_cast<const jbyte*>(data));
  return output_array;
}

// Convert a local to global reference, deleting the specified local reference.
jobject LocalToGlobalReference(JNIEnv* env, jobject local_reference) {
  jobject global_reference = nullptr;
  if (local_reference) {
    global_reference = env->NewGlobalRef(local_reference);
    env->DeleteLocalRef(local_reference);
  }
  return global_reference;
}

jobject ContinueBuilder(JNIEnv* env, jobject old_builder, jobject new_builder) {
  env->DeleteLocalRef(old_builder);
  return new_builder;
}

void RegisterCallbackOnTask(JNIEnv* env, jobject task,
                            TaskCallbackFn callback_fn, void* callback_data,
                            const char* api_identifier) {
  // Need to add the CallbackData to the g_task_callbacks before creating the
  // Java callback object as it could complete before we finish initializing
  // CallbackData.
  CallbackData* data;
  pthread_mutex_lock(&g_task_callbacks_mutex);
  {
    auto& callback_list = (*g_task_callbacks)[api_identifier];
    callback_list.push_back(CallbackData());
    auto list_it = callback_list.end();
    list_it--;
    data = &(*list_it);
    data->data = callback_data;
    data->callback_reference = nullptr;
    data->iterator = list_it;
    data->list = &callback_list;
    data->complete = false;
  }
  pthread_mutex_unlock(&g_task_callbacks_mutex);

  // Create JniResultCallback class to redirect the Java callback to C++.
  // Pointers are cast to jlongs, which are 64-bit integers.
  jobject jni_result_callback = env->NewObject(
      jniresultcallback::GetClass(),
      jniresultcallback::GetMethodId(jniresultcallback::kConstructor), task,
      reinterpret_cast<jlong>(callback_fn), reinterpret_cast<jlong>(data));

  // Store global reference to the callback so we can potentially cancel it in
  // Terminate().
  pthread_mutex_lock(&g_task_callbacks_mutex);
  // If the callback wasn't completed immediately
  // (see JniResultCallback_nativeOnResult()), add a global reference to the
  // callback so it can be completed otherwise, remove it from the list.
  if (data->complete) {
    data->list->erase(data->iterator);
  } else {
    data->callback_reference = env->NewGlobalRef(jni_result_callback);
  }
  pthread_mutex_unlock(&g_task_callbacks_mutex);

  // The jni_result_callback has registered itself with Task.
  // so it won't be garbage collected until the object has completed.
  env->DeleteLocalRef(jni_result_callback);
}

JNIEXPORT void JNICALL JniResultCallback_nativeOnResult(
    JNIEnv* env, jobject clazz, jobject result, jboolean success,
    jboolean cancelled, jstring status_message, jlong callback_fn_param,
    jlong callback_data) {
  void* user_callback_data;
  pthread_mutex_lock(&g_task_callbacks_mutex);
  {
    CallbackData* data = reinterpret_cast<CallbackData*>(callback_data);
    user_callback_data = data->data;
    // If a callback reference isn't present, the callback was completed before
    // RegisterCallbackOnTask() finished adding it to a
    // g_task_callbacks list.  In this case we leave it in the list and signal
    // RegisterCallbackOnTask() that it is complete by setting
    // complete to true.
    data->complete = true;
    if (data->callback_reference) {
      env->DeleteGlobalRef(data->callback_reference);
      data->list->erase(data->iterator);
    }
  }
  pthread_mutex_unlock(&g_task_callbacks_mutex);

  // Validate the assumption that it can't both succeed and be cancelled.
  assert(!(success && cancelled));

  // Cast the jlongs to pointers and call the C++ callbaack.
  std::string status_message_c = JStringToString(env, status_message);
  TaskCallbackFn* callback_fn =
      reinterpret_cast<TaskCallbackFn*>(callback_fn_param);
  FutureResult result_code =
      success ? kFutureResultSuccess
              : (cancelled ? kFutureResultCancelled : kFutureResultFailure);
  callback_fn(env, result, result_code, status_message_c.c_str(),
              user_callback_data);
}

// Call a C++ function pointer, passing in a given data pointer. This is called
// by the CppThreadDispatcher Java class, which uses the Java long type to
// contain C++ pointers.
JNIEXPORT void JNICALL CppThreadDispatcherContext_nativeFunction(
    JNIEnv* env, jobject clazz, jlong function_ptr, jlong function_data) {
  void (*func)(void*) = reinterpret_cast<void (*)(void*)>(function_ptr);
  func(reinterpret_cast<void*>(function_data));
}

int JavaThreadContext::initialize_count_ = 0;

JavaThreadContext::JavaThreadContext(JNIEnv* env) : object_(env) {}

JavaThreadContext::~JavaThreadContext() {}

void JavaThreadContext::Cancel() {
  JNIEnv* env = object_.GetJNIEnv();
  jobject dispatcher = object_.object();
  if (dispatcher) {
    env->CallVoidMethod(dispatcher, cppthreaddispatchercontext::GetMethodId(
                                        cppthreaddispatchercontext::kCancel));
    CheckAndClearJniExceptions(env);
  }
}

void JavaThreadContext::ReleaseExecuteCancelLock() {
  JNIEnv* env = object_.GetJNIEnv();
  jobject dispatcher = object_.object();
  if (dispatcher) {
    env->CallVoidMethod(
        dispatcher, cppthreaddispatchercontext::GetMethodId(
                        cppthreaddispatchercontext::kReleaseExecuteCancelLock));
    CheckAndClearJniExceptions(env);
  }
}

bool JavaThreadContext::AcquireExecuteCancelLock() {
  JNIEnv* env = object_.GetJNIEnv();
  bool acquired = false;
  jobject dispatcher = object_.object();
  if (dispatcher) {
    acquired =
        env->CallBooleanMethod(
            dispatcher,
            cppthreaddispatchercontext::GetMethodId(
                cppthreaddispatchercontext::kAcquireExecuteCancelLock)) != 0;
    CheckAndClearJniExceptions(env);
  }
  return acquired;
}

bool JavaThreadContext::Initialize(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files) {
  static const JNINativeMethod kCppThreadMethods[] = {
      {"nativeFunction", "(JJ)V",
       reinterpret_cast<void*>(CppThreadDispatcherContext_nativeFunction)}};

  if (!(cppthreaddispatchercontext::CacheClassFromFiles(env, activity_object,
                                                        &embedded_files) &&
        cppthreaddispatchercontext::CacheMethodIds(env, activity_object) &&
        cppthreaddispatchercontext::RegisterNatives(
            env, kCppThreadMethods, FIREBASE_ARRAYSIZE(kCppThreadMethods)) &&
        cppthreaddispatcher::CacheClassFromFiles(env, activity_object,
                                                 &embedded_files) &&
        cppthreaddispatcher::CacheMethodIds(env, activity_object))) {
    return false;
  }
  return true;
}

void JavaThreadContext::Terminate(JNIEnv* env) {
  cppthreaddispatchercontext::ReleaseClass(env);
  cppthreaddispatcher::ReleaseClass(env);
}

jobject JavaThreadContext::SetupInstance(JNIEnv* env, Callback function_ptr,
                                         void* function_data,
                                         Callback cancel_function_ptr,
                                         JavaThreadContext* context) {
  jobject java_context =
      env->NewObject(cppthreaddispatchercontext::GetClass(),
                     cppthreaddispatchercontext::GetMethodId(
                         cppthreaddispatchercontext::kConstructor),
                     reinterpret_cast<jlong>(function_ptr),
                     reinterpret_cast<jlong>(function_data),
                     reinterpret_cast<jlong>(cancel_function_ptr));
  CheckAndClearJniExceptions(env);
  if (context) context->object_.Set(java_context);
  return java_context;
}

void JavaThreadContext::RunOnMainThread(
    JNIEnv* env, jobject activity_object,
    JavaThreadContext::Callback function_ptr, void* function_data,
    JavaThreadContext::Callback cancel_function_ptr,
    JavaThreadContext* context) {
  jobject java_context = SetupInstance(env, function_ptr, function_data,
                                       cancel_function_ptr, context);
  env->CallStaticVoidMethod(
      cppthreaddispatcher::GetClass(),
      cppthreaddispatcher::GetMethodId(cppthreaddispatcher::kRunOnMainThread),
      activity_object, java_context);
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(java_context);
}

void JavaThreadContext::RunOnBackgroundThread(
    JNIEnv* env, JavaThreadContext::Callback function_ptr, void* function_data,
    JavaThreadContext::Callback cancel_function_ptr,
    JavaThreadContext* context) {
  jobject java_context = SetupInstance(env, function_ptr, function_data,
                                       cancel_function_ptr, context);
  env->CallStaticVoidMethod(cppthreaddispatcher::GetClass(),
                            cppthreaddispatcher::GetMethodId(
                                cppthreaddispatcher::kRunOnBackgroundThread),
                            java_context);
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(java_context);
}

void RunOnMainThread(JNIEnv* env, jobject activity_object,
                     JavaThreadContext::Callback function_ptr,
                     void* function_data,
                     JavaThreadContext::Callback cancel_function_ptr,
                     JavaThreadContext* context) {
  JavaThreadContext::RunOnMainThread(env, activity_object, function_ptr,
                                     function_data, cancel_function_ptr,
                                     context);
}

void RunOnBackgroundThread(JNIEnv* env,
                           JavaThreadContext::Callback function_ptr,
                           void* function_data,
                           JavaThreadContext::Callback cancel_function_ptr,
                           JavaThreadContext* context) {
  JavaThreadContext::RunOnBackgroundThread(env, function_ptr, function_data,
                                           cancel_function_ptr, context);
}

// Cancel all callbacks associated with the specified API identifier.  If an
// API identifier isn't specified, all pending callbacks are cancelled.
void CancelCallbacks(JNIEnv* env, const char* api_identifier) {
  LogDebug("Cancel pending callbacks for \"%s\"",
           api_identifier ? api_identifier : "<all>");
  for (;;) {
    bool pending_callbacks = false;
    jobject callback_reference = nullptr;
    pthread_mutex_lock(&g_task_callbacks_mutex);
    if (api_identifier) {
      auto& list = (*g_task_callbacks)[api_identifier];
      if (!list.empty()) {
        pending_callbacks = true;
        callback_reference = list.front().callback_reference;
      }
    } else {
      while (!g_task_callbacks->empty()) {
        auto map_it = g_task_callbacks->begin();
        auto& list = map_it->second;
        if (list.empty()) {
          g_task_callbacks->erase(map_it);
        } else {
          pending_callbacks = true;
          callback_reference = list.front().callback_reference;
          break;
        }
      }
    }
    if (pending_callbacks) {
      callback_reference = env->NewGlobalRef(callback_reference);
    }
    pthread_mutex_unlock(&g_task_callbacks_mutex);
    if (!pending_callbacks) break;

    // We can't call this while holding g_task_callbacks_mutex as this
    // could result in deadlock as cancel() and onCompletion() are both
    // synchronized on the JniResultCallback object.
    // This will remove the callback data entry from g_task_callbacks list.
    env->CallVoidMethod(callback_reference, jniresultcallback::GetMethodId(
                                                jniresultcallback::kCancel));
    CheckAndClearJniExceptions(env);
    env->DeleteGlobalRef(callback_reference);
  }
}

// Find a class and retrieve a global reference to it.
// NOTE: This method will assert if the class isn't found.
jclass FindClassGlobal(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>* embedded_files,
    const char* class_name, ClassRequirement optional) {
  LogDebug("Looking up class %s", class_name);
  jclass local_class = FindClass(env, class_name);
  if (!local_class && embedded_files) {
    local_class =
        FindClassInFiles(env, activity_object, *embedded_files, class_name);
  }
  LogDebug("Class %s, lref 0x%08x", class_name,
           static_cast<int>(reinterpret_cast<intptr_t>(local_class)));
  if (!local_class) {
    if (optional == kClassRequired) {
      LogError(kMissingJavaClassError, class_name, class_name);
    }
    return nullptr;
  }
  jclass global_class = static_cast<jclass>(env->NewGlobalRef(local_class));
  env->DeleteLocalRef(local_class);
  LogDebug("Class %s, gref 0x%08x", class_name,
           static_cast<int>(reinterpret_cast<intptr_t>(global_class)));
  CheckAndClearJniExceptions(env);
  if (!global_class) {
    if (optional == kClassRequired) {
      LogError(kMissingJavaClassError, class_name, class_name);
    }
    return nullptr;
  }
  return global_class;
}

// Find a class, attempting to load the class if it's not found.
jclass FindClass(JNIEnv* env, const char* class_name) {
  jclass class_object = env->FindClass(class_name);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    // If the class isn't found it's possible NativeActivity is being used by
    // the application which means the class path is set to only load system
    // classes.  The following falls back to loading the class using the
    // Activity and Dex classes loaders before retrieving a reference to it.
    class_object = FindOrLoadClassFromLoaders(env, class_name);
  }
  return class_object;
}

// Cache a list of embedded files to the activity's cache directory.
const std::vector<internal::EmbeddedFile>& CacheEmbeddedFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files) {
  jobject cache_dir = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetCacheDir));
  CheckAndClearJniExceptions(env);
  // Write each file in the resources to the cache.
  for (std::vector<internal::EmbeddedFile>::const_iterator it =
           embedded_files.begin();
       it != embedded_files.end(); ++it) {
    LogDebug("Caching %s", it->name);
    jstring filename = env->NewStringUTF(it->name);
    jobject output_file = env->NewObject(
        file::GetClass(), file::GetMethodId(file::kConstructorFilePath),
        cache_dir, filename);
    env->DeleteLocalRef(filename);
    jobject output_stream = env->NewObject(
        file_output_stream::GetClass(),
        file_output_stream::GetMethodId(file_output_stream::kConstructorFile),
        output_file);
    bool failed = CheckAndClearJniExceptions(env);
    if (!failed) {
      jobject output_array = env->NewByteArray(it->size);
      env->SetByteArrayRegion(static_cast<jbyteArray>(output_array), 0,
                              it->size,
                              reinterpret_cast<const jbyte*>(it->data));
      env->CallVoidMethod(
          output_stream,
          file_output_stream::GetMethodId(file_output_stream::kWrite),
          output_array, 0, it->size);
      failed |= CheckAndClearJniExceptions(env);
      env->CallVoidMethod(output_stream, file_output_stream::GetMethodId(
                                             file_output_stream::kClose));
      failed |= CheckAndClearJniExceptions(env);
      env->DeleteLocalRef(output_array);
      env->DeleteLocalRef(output_stream);
    }
    env->DeleteLocalRef(output_file);
    if (failed) {
      LogError(
          "Unable to cache file %s, embedded Java class loading will "
          "fail.  It is likely the device is out of space for "
          "application data storage, free some space and try again.",
          it->name);
      break;
    }
  }
  env->DeleteLocalRef(cache_dir);
  return embedded_files;
}

// Attempt to load a class from a set of files which have been cached to local
// storage using CacheEmbeddedFiles().
jclass FindClassInFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files,
    const char* class_name) {
  if (!embedded_files.size()) {
    return nullptr;
  }

  jobject cache_dir = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetCacheDir));
  CheckAndClearJniExceptions(env);
  jobject cache_dir_path_jstring = env->CallObjectMethod(
      cache_dir, file::GetMethodId(file::kGetAbsolutePath));
  CheckAndClearJniExceptions(env);
  std::string cache_dir_path =
      FIREBASE_NAMESPACE::util::JniStringToString(env, cache_dir_path_jstring);

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  static const char kPathSeparator = '/';

  jobject cache_uri =
      env->CallObjectMethod(cache_dir, file::GetMethodId(file::kToUri));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(cache_dir);
  jobject cache_url =
      env->CallObjectMethod(cache_uri, java_uri::GetMethodId(java_uri::kToUrl));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(cache_uri);
  jobjectArray url_path_array =
      env->NewObjectArray(embedded_files.size(), url::GetClass(), nullptr);
  for (int i = 0; i < embedded_files.size(); ++i) {
    jstring embedded_file_string = env->NewStringUTF(embedded_files[i].name);
    jobject jar_url =
        env->NewObject(url::GetClass(), url::GetMethodId(url::kConstructor),
                       cache_url, embedded_file_string);
    env->SetObjectArrayElement(url_path_array, i, jar_url);
    env->DeleteLocalRef(jar_url);
    env->DeleteLocalRef(embedded_file_string);
  }
  env->DeleteLocalRef(cache_url);

  jobject class_loader_obj = env->NewObject(
      url_class_loader::GetClass(),
      url_class_loader::GetMethodId(url_class_loader::kConstructor),
      url_path_array, GetParentLoader());
  env->DeleteLocalRef(url_path_array);

  std::string class_name_str(class_name);
  std::replace(class_name_str.begin(), class_name_str.end(), kPathSeparator,
               '.');
  LogDebug("Load class %s (a.k.a. %s)", class_name_str.c_str(), class_name);
  jstring class_name_object = env->NewStringUTF(class_name_str.c_str());
  jclass loaded_class = static_cast<jclass>(env->CallObjectMethod(
      class_loader_obj,
      url_class_loader::GetMethodId(url_class_loader::kLoadClass),
      class_name_object));
  CheckAndClearJniExceptions(env);
#else   // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  static const char kPathSeparator = '/';
  static const char kDexPathSeparator = ':';
  // Older versions of Android don't have GetCodeCacheDir, so fall back to
  // GetCacheDir.
  const jmethodID get_code_cache_dir_method_id =
      activity::GetMethodId(activity::kGetCodeCacheDir) != 0
          ? activity::GetMethodId(activity::kGetCodeCacheDir)
          : activity::GetMethodId(activity::kGetCacheDir);
  jobject code_cache_dir =
      env->CallObjectMethod(activity_object, get_code_cache_dir_method_id);
  CheckAndClearJniExceptions(env);
  jobject code_cache_dir_path = env->CallObjectMethod(
      code_cache_dir, file::GetMethodId(file::kGetAbsolutePath));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(code_cache_dir);
  env->DeleteLocalRef(cache_dir);

  std::string dex_path;
  for (std::vector<internal::EmbeddedFile>::const_iterator it =
           embedded_files.begin();
       it != embedded_files.end(); ++it) {
    dex_path += cache_dir_path + kPathSeparator + std::string(it->name);
    dex_path.push_back(kDexPathSeparator);
  }
  dex_path.pop_back();

  LogDebug("Set class path to %s", dex_path.c_str());

  jstring dex_path_string = env->NewStringUTF(dex_path.c_str());
  jobject class_loader_obj = env->NewObject(
      dex_class_loader::GetClass(),
      dex_class_loader::GetMethodId(dex_class_loader::kConstructor),
      dex_path_string, code_cache_dir_path, nullptr, GetParentLoader());
  env->DeleteLocalRef(code_cache_dir_path);
  env->DeleteLocalRef(dex_path_string);

  LogDebug("Load class %s", class_name);
  jstring class_name_object = env->NewStringUTF(class_name);
  jclass loaded_class = static_cast<jclass>(env->CallObjectMethod(
      class_loader_obj,
      dex_class_loader::GetMethodId(dex_class_loader::kLoadClass),
      class_name_object));
  CheckAndClearJniExceptions(env);
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

  if (!env->ExceptionCheck()) {
    LogDebug("%s loaded.", class_name);
    AddClassLoader(env, class_loader_obj);
  } else {
    env->ExceptionClear();
    LogDebug("%s *not* loaded", class_name);
    env->DeleteLocalRef(loaded_class);
    env->DeleteLocalRef(class_loader_obj);
  }
  env->DeleteLocalRef(class_name_object);
  return loaded_class;
}

// Get a resource ID from the activity's package.
int GetResourceIdFromActivity(JNIEnv* env, jobject activity_object,
                              const char* resource_name,
                              ResourceType resource_type) {
  jobject resources_object = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetResources));
  CheckAndClearJniExceptions(env);
  jobject package_name = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetPackageName));
  CheckAndClearJniExceptions(env);
  jobject resource_type_string =
      env->NewStringUTF(kResourceTypeStrings[resource_type]);
  jobject resource_name_string = env->NewStringUTF(resource_name);
  int resource_id = env->CallIntMethod(
      resources_object, resources::GetMethodId(resources::kGetIdentifier),
      resource_name_string, resource_type_string, package_name);
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(resource_name_string);
  env->DeleteLocalRef(resource_type_string);
  env->DeleteLocalRef(package_name);
  env->DeleteLocalRef(resources_object);
  return resource_id;
}

// Get a resource value as a string from the activity's package.
std::string GetResourceStringFromActivity(JNIEnv* env, jobject activity_object,
                                          int resource_id) {
  FIREBASE_ASSERT(resource_id);
  jobject resource_value_string = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetString),
      resource_id);
  CheckAndClearJniExceptions(env);
  return JniStringToString(env, resource_value_string);
}

// Get the name of the package associated with this activity.
std::string GetPackageName(JNIEnv* env, jobject activity_object) {
  jobject package_name_string = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetPackageName));
  CheckAndClearJniExceptions(env);
  return JniStringToString(env, package_name_string);
}

// Check for JNI exceptions, report them if any, and clear them.
bool CheckAndClearJniExceptions(JNIEnv* env) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return true;
  }
  return false;
}

std::string GetAndClearExceptionMessage(JNIEnv* env) {
  jobject exception = env->ExceptionOccurred();
  if (exception) {
    env->ExceptionClear();
    std::string exception_message_out = GetMessageFromException(env, exception);
    env->DeleteLocalRef(exception);
    return exception_message_out;
  }
  return std::string();
}

std::string GetMessageFromException(JNIEnv* env, jobject exception) {
  if (exception != nullptr) {
    jstring message = static_cast<jstring>(env->CallObjectMethod(
        exception, throwable::GetMethodId(throwable::kGetLocalizedMessage)));
    CheckAndClearJniExceptions(env);
    if (!message) {
      // If GetLocalizedMessage returns null, try GetMessage.
      message = static_cast<jstring>(env->CallObjectMethod(
          exception, throwable::GetMethodId(throwable::kGetMessage)));
      CheckAndClearJniExceptions(env);
    }
    if (!message || env->GetStringUTFLength(message) == 0) {
      // If GetMessage returns null, just use ToString.
      if (message) {
        // If it was an empty string, we'll need to free the message ref.
        env->DeleteLocalRef(message);
      }
      message = static_cast<jstring>(env->CallObjectMethod(
          exception, throwable::GetMethodId(throwable::kToString)));
      CheckAndClearJniExceptions(env);
    }
    if (message) {
      return JniStringToString(env, message);
    } else {
      return std::string("Unknown Exception.");
    }
  }
  return std::string();
}

bool LogException(JNIEnv* env, LogLevel log_level, const char* log_fmt, ...) {
  jobject exception = env->ExceptionOccurred();
  if (exception != nullptr) {
    env->ExceptionClear();
    jobject message = env->CallObjectMethod(
        exception, throwable::GetMethodId(throwable::kGetLocalizedMessage));
    CheckAndClearJniExceptions(env);
    if (!message) {
      // If GetLocalizedMessage returns null, try GetMessage.
      message = env->CallObjectMethod(
          exception, throwable::GetMethodId(throwable::kGetMessage));
      CheckAndClearJniExceptions(env);
    }
    if (!message) {
      // If GetMessage returns null too, just use ToString.
      message = env->CallObjectMethod(
          exception, throwable::GetMethodId(throwable::kToString));
      CheckAndClearJniExceptions(env);
    }
    if (message) {
      std::string message_str = JniStringToString(env, message);
      if (log_fmt == nullptr) {
        LogMessage(log_level, "%s", message_str.c_str());
      } else {
        static char buf[512];
        va_list list;
        va_start(list, log_fmt);
        vsnprintf(buf, sizeof(buf) - 1, log_fmt, list);
        va_end(list);
        strncat(buf, ": ", sizeof(buf) - 1);
        strncat(buf, message_str.c_str(), sizeof(buf) - 1);
        LogMessage(log_level, "%s", buf);
      }
    }
    env->DeleteLocalRef(exception);
    return true;
  }
  return false;
}

// Static variables used in tracking thread initialization and cleanup.
pthread_key_t jni_env_key;
pthread_once_t pthread_key_initialized = PTHREAD_ONCE_INIT;

// The following methods are extern "C" to ensure they're called with the
// C rather than C++ linkage convention.
namespace {

extern "C" void DetachJVMThreads(void* stored_java_vm) {
  assert(stored_java_vm);
  JNIEnv* jni_env;
  JavaVM* java_vm = static_cast<JavaVM*>(stored_java_vm);
  // AttachCurrentThread does nothing if we're already attached, but
  // calling it ensures that the DetachCurrentThread doesn't fail.
  FIREBASE_NAMESPACE::util::AttachCurrentThread(java_vm, &jni_env);
  java_vm->DetachCurrentThread();
}

// Called the first time GetJNIEnv is invoked.
// Ensures that jni_env_key is created and that the destructor is in place.
extern "C" void SetupJvmDetachOnThreadDestruction() {
  pthread_key_create(&jni_env_key, DetachJVMThreads);
}

}  // namespace

JNIEnv* GetThreadsafeJNIEnv(JavaVM* java_vm) {
  // Set up the thread key and destructor the first time this is called:
  (void)pthread_once(&pthread_key_initialized,
                     SetupJvmDetachOnThreadDestruction);
  pthread_setspecific(jni_env_key, java_vm);

  JNIEnv* env;
  jint result = FIREBASE_NAMESPACE::util::AttachCurrentThread(java_vm, &env);
  return result == JNI_OK ? env : nullptr;
}

jint AttachCurrentThread(JavaVM* java_vm, JNIEnv** env) {
  return java_vm->AttachCurrentThread(
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
      reinterpret_cast<void**>(env),
#else   // defined(FIREBASE_ANDROID_FOR_DESKTOP)
      env,
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
      nullptr);
}

// Returns a pointer to the JNI environment. This retrieves the JNIEnv from
// firebase::App, either the default App (if it exists) or any valid App.
JNIEnv* GetJNIEnvFromApp() {
  App* app = app_common::GetDefaultApp();
  if (!app) app = app_common::GetAnyApp();
  return app ? app->GetJNIEnv() : nullptr;
}

}  // namespace util
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
